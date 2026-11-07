"""Exercise the dynamic loader with synthetic C ABI libraries, not model weights."""
import os
import asyncio
import pathlib
import shutil
import subprocess
import sys
import tempfile
from unittest.mock import patch

probe, full, base, registry, missing, server = map(pathlib.Path, sys.argv[1:])
suffix = full.suffix
prefix = '' if suffix == '.dll' else 'lib'
with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp)
    model = root/'fixture.gguf';model.write_bytes(b'fixture-not-a-model')
    pack = root/'native pack';pack.mkdir()
    marker = root/'model-loaded'
    for name, library in (('ggml-base',base),('ggml',registry),('llama',full)):
        shutil.copy2(library, pack/f'{prefix}{name}{suffix}')
    def run(mode='ok', layers=0, error=None):
        marker.unlink(missing_ok=True)
        result = subprocess.run([str(probe),str(model),str(pack),mode,str(layers)],
            env={**os.environ,'WDS_PACK_TEST_CASE':mode,'WDS_PACK_TEST_MODEL_MARKER':str(marker)},
            text=True,capture_output=True,timeout=20)
        if error:
            assert result.returncode!=0 and error in result.stderr, result.stdout+result.stderr
        else:
            assert result.returncode==0,result.stdout+result.stderr
        return result
    run();run('limit')
    result=run('gpu-fixture',layers=99)
    assert 'Native GPU backend: synthetic-device-fixture' in result.stderr,result.stderr
    run('bad-abi',error='runtime ABI mismatch');assert not marker.exists()
    run('null-version',error='runtime ABI mismatch');assert not marker.exists()
    run(layers=99,error='registered no GPU device');assert not marker.exists()
    run('decode-fails',error='representation evaluation failed')
    run('missing-output',error='did not return a pooled representation')
    # Check that a real child startup failure is surfaced by the Python supervisor,
    # with the same retained diagnostic. The child fails before opening its socket.
    project=pathlib.Path(__file__).resolve().parents[1]
    sys.path.insert(0,str(project/'tools'))
    from workshop import Server
    config={'port':18765,'data':str(root/'data'),'runs':str(root/'runs'),
            'model':str(model),'runtime_pack':str(pack),'gpu_layers':0,
            'initial_package':str(project/'packages/breeze'),'package_root':str(project/'packages')}
    async def startup_error():
        child=Server(server,config)
        try:
            try:await child.start()
            except RuntimeError as error:
                assert 'runtime ABI mismatch' in str(error),str(error)
                assert 'runtime ABI mismatch' in (root/'runs/logs/runtime.log').read_text()
            else:raise AssertionError('invalid runtime started')
        finally:await child.stop()
    with patch.dict(os.environ,{'WDS_PACK_TEST_CASE':'bad-abi'}):asyncio.run(startup_error())
    shutil.copy2(missing,pack/f'{prefix}llama{suffix}')
    run(error='missing required b9870 symbol');assert not marker.exists()
    (pack/f'{prefix}llama{suffix}').unlink()
    run(error='cannot load runtime library');assert not marker.exists()
print('Split-library export ownership, synthetic device name lookup, ABI rejection before model load, missing symbols/libraries, GPU requirement, decode failures and explicit limits passed; no real GPU or model was tested here.')
