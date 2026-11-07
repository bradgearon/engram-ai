#!/usr/bin/env python3
"""Configure bounded local traces and export a diagnostic ZIP (no model/runtime binaries)."""
import argparse, hashlib, json, logging, logging.handlers, pathlib, platform, re, sys, time, uuid, zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULTS = dict(enabled=False, level='trace', include_content=True, max_file_mb=16, keep_files=8)

def settings(config):
    value = config.get('diagnostics', {})
    if not isinstance(value, dict) or set(value) - set(DEFAULTS):
        raise ValueError('Invalid diagnostics configuration keys')
    out = {**DEFAULTS, **value}
    for key in ('enabled', 'include_content'):
        if type(out[key]) is not bool: raise ValueError('diagnostics.'+key+' must be boolean')
    if out['level'] not in ('errors', 'trace'): raise ValueError('diagnostics.level must be errors or trace')
    for key, upper in (('max_file_mb', 256), ('keep_files', 32)):
        if type(out[key]) is not int or not 1 <= out[key] <= upper: raise ValueError('Invalid diagnostics.'+key)
    return out

def redact(value):
    if isinstance(value, dict):
        return {k: ('[REDACTED]' if any(word in k.lower() for word in ('password', 'secret', 'api_key', 'authorization', 'access_token')) else redact(v)) for k,v in value.items()}
    if isinstance(value, list): return [redact(v) for v in value]
    return value

class TraceHandler(logging.handlers.RotatingFileHandler):
    def doRollover(self):
        expired = pathlib.Path(self.baseFilename+'.'+str(self.backupCount)).exists()
        super().doRollover()
        self.stream.write(json.dumps({'schema':1,'time_ms':int(time.time()*1000),'kind':'trace.rotation',
                                     'retention_expired':expired,'keep_files':self.backupCount+1})+'\n')
        self.stream.flush()
    def handleError(self, record):
        # Logging normally swallows I/O errors. Make loss explicit in supervisor stderr.
        print('DIAGNOSTICS FAILURE: could not write '+self.baseFilename, file=sys.stderr, flush=True)

class Recorder:
    def __init__(self, config, source):
        self.options = settings(config); self.source=source; self.session=uuid.uuid4().hex; self.sequence=0
        self.handler=None
        if self.options['enabled']:
            directory=ROOT/config['runs']/'logs'/'diagnostics'; directory.mkdir(parents=True,exist_ok=True)
            self.handler=TraceHandler(directory/(source+'.jsonl'),maxBytes=self.options['max_file_mb']*1024*1024,
                                      backupCount=self.options['keep_files'],encoding='utf-8')
    def record(self, kind, payload=None, error=False):
        if not self.handler or (self.options['level']=='errors' and not error): return
        try:
            self.sequence+=1
            if not self.options['include_content']:
                payload={'content_omitted':True}
            row=dict(schema=1,time_ms=int(time.time()*1000),session_id=self.session,sequence=self.sequence,
                     source=self.source,kind=kind,payload=redact(payload))
            self.handler.handle(logging.LogRecord(self.source,logging.ERROR if error else logging.INFO,'',0,
                                json.dumps(row,ensure_ascii=True,default=str),(),None))
        except Exception as exc:
            print('DIAGNOSTICS FAILURE: '+str(exc),file=sys.stderr,flush=True)
    def close(self):
        if self.handler:self.handler.close()

def bundle(config, output):
    output=pathlib.Path(output).resolve(); output.parent.mkdir(parents=True,exist_ok=True)
    options=settings(config)
    manifest={'schema':1,'created_ms':int(time.time()*1000),'platform':platform.platform(),
              'python':platform.python_version(),'diagnostics':options,'files':[],
              'notes':['Full traces may contain conversation content and local paths.',
                       'Logs retain a bounded rolling window. trace.rotation records indicate expired files.',
                       'Journal snapshots contain complete lines at the recorded byte cutoff. Stop first for a quiescent snapshot.',
                       'GGUF files, DLLs, executables, build directories and environment variables are excluded.']}
    def add_bytes(z,name,data):
        z.writestr(name,data);manifest['files'].append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    def add_file(z,path,name,journal=False):
        # Stream to avoid loading a large authoritative chain into memory.
        try:
            with path.open('rb') as f, z.open(name,'w') as dest:
                cutoff=path.stat().st_size; remaining=cutoff; h=hashlib.sha256(); count=0
                while remaining:
                    data=f.readline(remaining) if journal else f.read(min(1024*1024,remaining))
                    if not data:break
                    remaining-=len(data)
                    if journal and not data.endswith(b'\n'):break
                    dest.write(data);h.update(data);count+=len(data)
                manifest['files'].append({'path':name,'bytes':count,'source_bytes_at_snapshot':cutoff,
                                          'omitted_tail_bytes':cutoff-count,'sha256':h.hexdigest()})
        except OSError as e:manifest.setdefault('warnings',[]).append({'path':name,'error':str(e)})
    temporary=output.with_name(output.name+'.tmp')
    try:
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED) as z:
            add_bytes(z,'config.json',(json.dumps(redact(config),indent=2)+'\n').encode())
            for name in ('RELEASE_MANIFEST.json','CMakeLists.txt','ui/package.json'):
                if (ROOT/name).is_file():add_file(z,ROOT/name,name)
            logs=ROOT/config['runs']/'logs'
            if logs.is_dir():
                for file in sorted(logs.rglob('*')):
                    if file.is_file() and re.fullmatch(r'.+\.(?:log|jsonl)(?:\.\d+)?',file.name):
                        add_file(z,file,'logs/'+file.relative_to(logs).as_posix(),'.jsonl' in file.name)
            if options['include_content']:
                for role in ('runtime','teacher'):
                    chain=ROOT/config['data']/role/'events.jsonl'
                    if chain.is_file():add_file(z,chain,role+'/events.jsonl',True)
            else:manifest['notes'].append('Journal omitted because include_content=false. Older retained logs may still contain content recorded earlier.')
            packages=ROOT/config.get('package_root','packages')
            if packages.is_dir():
                for file in sorted(packages.rglob('manifest.json')):
                    add_file(z,file,'packages/'+file.relative_to(packages).as_posix())
            z.writestr('diagnostics-manifest.json',json.dumps(manifest,indent=2)+'\n')
        temporary.replace(output)
    finally:
        if temporary.exists():temporary.unlink()
    return output

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',default='configs/sixstars.local.json')
    sub=parser.add_subparsers(dest='action',required=True)
    enable=sub.add_parser('enable');enable.add_argument('--metadata-only',action='store_true')
    sub.add_parser('disable')
    export=sub.add_parser('bundle');export.add_argument('--output',default='wds-diagnostics.zip')
    args=parser.parse_args(); path=ROOT/args.config; config=json.loads(path.read_text(encoding='utf-8-sig'))
    if args.action=='bundle':print(bundle(config,args.output));return
    config['diagnostics']={**settings(config),'enabled':args.action=='enable'}
    if args.action=='enable':config['diagnostics']['include_content']=not args.metadata_only
    temporary=path.with_suffix(path.suffix+'.tmp');temporary.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8');temporary.replace(path)
    print('Diagnostics '+('enabled' if args.action=='enable' else 'disabled')+'. Restart the workshop to apply. Model, runtime-pack and data paths retained.')

if __name__=='__main__':
    try:main()
    except Exception as error:print('Diagnostics error: '+str(error),file=sys.stderr);sys.exit(1)
