"""Manifest/path/setup checks. Fixture headers and EXEs are never used for inference."""
import argparse
import asyncio
import contextlib
import io
import json
import pathlib
import shutil
import sys
import tempfile
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from configure_sixstars import configure, installation, runtime_details
from workshop import configuration, main, build
from runtime_pack import configure_pack


def rejects(action, phrase):
    try:
        action()
    except ValueError as error:
        assert phrase in str(error), str(error)
    else:
        raise AssertionError('invalid setup accepted')


with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stdout(io.StringIO()):
    root = pathlib.Path(tmp) / 'six-stars-solution-2'
    shutil.copytree(ROOT / 'tests/fixtures/sixstars', root)
    model = root / 'models/llm/gemma-4-E4B-it-Q6_K.gguf'
    model.parent.mkdir(parents=True)
    args = argparse.Namespace(six_stars=str(root), models_root=None, model=None,
        cuda=False, gpu_layers=None, llama_source=None, output=str(root/'output.json'),
        replace=False, reuse=False, port=8765)
    rejects(lambda: configure(args), 'GGUF not found')
    assert not (root/'output.json').exists()
    model.write_bytes(b'GGUF-fixture-only')
    for location in (root, root/'config', root/'config/models.json', root/'config/runtime-manifest.json'):
        assert installation(location)[0] == root
    configure(args)
    config = json.loads((root/'output.json').read_text())
    assert config['model'] == str(model) and config['llama'] and not config['cuda']
    args.cuda = True; args.replace = True
    configure(args)
    assert json.loads((root/'output.json').read_text())['gpu_layers'] == 99
    # Preserve customized existing WDS paths and GPU settings when starting again.
    config = json.loads((root/'output.json').read_text())
    config.update(data='my-existing-data', port=17765)
    (root/'output.json').write_text(json.dumps(config))
    old = (root/'output.json').read_bytes()
    args.cuda = False; args.replace = False; args.reuse = True
    configure(args)
    assert (root/'output.json').read_bytes() == old
    args.gpu_layers = 42; args.cuda = True
    rejects(lambda: configure(args), 'Existing gpu_layers differs')
    args.gpu_layers = None; args.cuda = False
    # The runtime manifest's installed path and the visible runtime-pack layout are distinct.
    pack = root/'runtime-packs/win-x64/llama-cpp-dynamic-win-x64/llama-server.exe'
    pack.parent.mkdir(parents=True); pack.write_bytes(b'fixture-not-an-executable')
    report = runtime_details(root)
    assert report['detected_path'] == str(pack) and not report['used_by_wds']
    engine = root/'engines/llm/llama-cpp-dynamic-win-x64/llama-server.exe'
    engine.parent.mkdir(parents=True); engine.write_bytes(b'fixture-not-an-executable')
    assert runtime_details(root)['detected_path'] == str(engine)
    # Model relocation and Shell Link target joining (COM is not executed on Linux).
    moved = root/'relocated/llm'/model.name
    moved.parent.mkdir(parents=True); model.rename(moved)
    args.models_root = str(root/'relocated')
    rejects(lambda: configure(args), 'another configuration/model')
    args.replace = True
    configure(args)
    assert json.loads((root/'output.json').read_text())['model'] == str(moved)
    args.models_root = None
    (root/'models.lnk').write_bytes(b'fixture-link')
    with patch('configure_sixstars.shortcut_target', return_value=root/'relocated'):
        configure(args)
    args.model = str(pack)
    rejects(lambda: configure(args), 'not a GGUF')
    args.model = str(moved); args.llama_source = str(pack.parent)
    rejects(lambda: configure(args), 'source checkout')
    args.llama_source = None
    rejects(lambda: configuration(root/'config/models.json'), 'Six Stars manifest')
    rejects(lambda: configuration(root/'absent.json'), 'generated separately')
    # Exercise the one-command launcher setup without treating a fixture header as a real model.
    async def fake_start(parsed, actual):
        assert actual['model'] == str(moved) and actual['data'] == 'sixstars-wds-data'
        assert parsed.ui and actual['llama']
    launch = argparse.Namespace(command='start', six_stars=str(root/'config'),
        models_root=str(root/'relocated'), model=None, cuda=False, gpu_layers=None,
        config=str(root/'launch.json'), ui=True)
    with patch('workshop.start', fake_start):
        asyncio.run(main(launch))
        asyncio.run(main(launch))
    assert json.loads((root/'launch.json').read_text())['model'] == str(moved)
    # Switch an existing GPU source build to the user's DLL pack without resetting
    # its model, data, package selection, ports or training history.
    native=pack.parent
    for name in ('llama.dll','ggml.dll','ggml-base.dll','ggml-cpu-x64.dll','ggml-cuda.dll'):
        (native/name).write_bytes(b'fixture-only-not-inference')
    before=json.loads((root/'launch.json').read_text())
    before.update(data='keep-my-chats',runs='keep-my-training',gpu_layers=99,cuda=True,port=18765)
    (root/'launch.json').write_text(json.dumps(before))
    launch.six_stars=None;launch.models_root=None;launch.runtime_pack=str(native)
    async def pack_start(parsed, actual):
        for key in ('model','data','runs','port','gpu_layers','initial_package','package_root'):
            assert actual[key]==before[key],key
        assert actual['runtime_pack']==str(native) and actual['build_dir']=='build-native-pack'
        assert not actual['llama'] and not actual['ggml'] and not actual['cuda']
        assert actual['trainer_backend']=='scalar-sgd'
    with patch('workshop.start',pack_start):asyncio.run(main(launch))
    assert configuration(root/'launch.json')['runtime_pack']==str(native)
    # A requested pack relocation must override a stale path before validation.
    stale=json.loads((root/'launch.json').read_text());stale['runtime_pack']=str(root/'missing-pack')
    (root/'launch.json').write_text(json.dumps(stale))
    with patch('workshop.start',pack_start):asyncio.run(main(launch))
    (native/'ggml-cuda.dll').unlink()
    rejects(lambda:configure_pack(before,native),'no GGML CUDA backend')
    (native/'ggml-cuda.dll').write_bytes(b'fixture')
    (native/'llama.dll').unlink()
    rejects(lambda:configure_pack(before,native),'native libraries')
    (native/'llama.dll').write_bytes(b'fixture')
    # Inspect real CMake invocations through the launcher, avoiding a fake GPU run.
    calls=[]
    async def capture(*args):calls.append([str(x) for x in args])
    selected=configure_pack(before,native);selected['build_dir']=str(root/'native-build')
    with patch('workshop.command',capture),patch('workshop.shutil.which',return_value='cmake'):
        asyncio.run(build(selected))
    options=calls[0]
    for required in ('-DWDS_RUNTIME_PACK=ON','-DWDS_LLAMA=OFF','-DWDS_GGML_TRAIN=OFF','-DGGML_CUDA=OFF'):
        assert required in options,options
    assert not any('CUDA_ARCHITECTURES' in x for x in options)
    calls.clear();before['build_dir']=str(root/'source-build')
    with patch('workshop.command',capture),patch('workshop.shutil.which',return_value='cmake'):
        asyncio.run(build(before))
    assert '-DCMAKE_CUDA_ARCHITECTURES=native' in calls[0]
print('Six Stars supplied manifests, root/config paths, pack diagnostics, model relocation, shortcut target joining, safe reuse and one-command setup passed')
