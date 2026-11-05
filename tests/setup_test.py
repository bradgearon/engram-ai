"""Path resolution/setup only; fake headers are not claimed as working language models."""
import argparse,json,os,pathlib,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from configure_sixstars import configure
with tempfile.TemporaryDirectory() as tmp:
    root=pathlib.Path(tmp);(root/'config').mkdir();(root/'models/llm').mkdir(parents=True)
    (root/'config/models.json').write_text(json.dumps({'models':{'llm':{'localPath':'models/llm/fixture.gguf'}}}))
    args=argparse.Namespace(six_stars=str(root),models_root=None,model=None,cuda=False,gpu_layers=None,llama_source=None,output=str(root/'output.json'),replace=False,port=8765)
    try:configure(args);raise AssertionError('missing model accepted')
    except ValueError:pass
    model=root/'models/llm/fixture.gguf';model.write_bytes(b'GGUF-fixture-only')
    configure(args);config=json.loads((root/'output.json').read_text());assert config['model']==str(model) and config['llama'] and not config['cuda']
    args.cuda=True;args.replace=True;configure(args);assert json.loads((root/'output.json').read_text())['gpu_layers']==99
    args.models_root=str(root/'relocated');(root/'relocated/llm').mkdir(parents=True);model2=root/'relocated/llm/fixture.gguf';model2.write_bytes(b'GGUF-test')
    configure(args);assert json.loads((root/'output.json').read_text())['model']==str(model2)
print('Six Stars model-path resolution and explicit missing-file failure passed')
