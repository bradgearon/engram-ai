#!/usr/bin/env python3
"""Reuse Six Stars' local LLM GGUF; build wds.ai's own pinned native llama.cpp adapter."""
import argparse,json,os,pathlib,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]

def configure(args):
    base=pathlib.Path(args.six_stars).expanduser().resolve()
    manifest=base/'config/models.json'
    model=json.loads(manifest.read_text(encoding='utf-8-sig'))['models']['llm']['localPath']
    relative=pathlib.Path(model)
    models_root=args.models_root or os.environ.get('SIX_STARS_MODELS_ROOT')
    if args.model:target=pathlib.Path(args.model).expanduser().resolve()
    elif relative.is_absolute():target=relative
    elif models_root:
        configured=pathlib.Path(models_root).expanduser()
        if not configured.is_absolute():configured=base/configured
        parts=relative.parts
        if parts and parts[0].lower()=='models':parts=parts[1:]
        target=(configured/pathlib.Path(*parts)).resolve()
    else:target=(base/relative).resolve()
    if not target.is_file():raise ValueError('GGUF not found: '+str(target)+'. In Six Stars, run launch.cmd --download-models-only, or pass --model with its existing GGUF path.')
    with target.open('rb') as file:
        if file.read(4)!=b'GGUF':raise ValueError('Selected file is not a GGUF: '+str(target))
    config=json.loads((ROOT/'configs/local.json').read_text())
    config.update({'build_dir':'build-sixstars','model':str(target),'llama':True,'ggml':True,
        'cuda':args.cuda,'gpu_layers':args.gpu_layers if args.gpu_layers is not None else (99 if args.cuda else 0),
        'data':'sixstars-wds-data','runs':'sixstars-wds-runs','port':args.port})
    if config['gpu_layers']<0 or (config['gpu_layers']>0 and not args.cuda):raise ValueError('GPU layers require --cuda in this setup helper; CPU uses 0.')
    if args.llama_source:
        source=pathlib.Path(args.llama_source).expanduser().resolve()
        if not (source/'include/llama.h').is_file():raise ValueError('--llama-source must be a llama.cpp source checkout, not the engine binaries folder')
        config['llama_source']=str(source)
    output=(ROOT/args.output).resolve()
    if output.exists() and not args.replace:raise ValueError('Configuration exists; use --replace explicitly or choose --output.')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(config,indent=2)+'\n')
    print(json.dumps({'config':str(output),'gguf':str(target),'backend':'CUDA source build' if args.cuda else 'CPU source build',
        'reuses':'model file only; no model copy, no Six Stars binary replacement','start':['python','tools/workshop.py','--config',str(output),'start','--upkeep']},indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--six-stars',required=True);p.add_argument('--models-root');p.add_argument('--model');p.add_argument('--llama-source')
    p.add_argument('--cuda',action='store_true');p.add_argument('--gpu-layers',type=int);p.add_argument('--port',type=int,default=8765)
    p.add_argument('--output',default='configs/sixstars.local.json');p.add_argument('--replace',action='store_true')
    try:configure(p.parse_args())
    except Exception as error:print(str(error),file=sys.stderr);sys.exit(1)
