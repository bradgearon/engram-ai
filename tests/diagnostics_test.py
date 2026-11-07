"""Backend-only checks: native telemetry, same-origin browser ingestion, rotation and exports."""
import asyncio, hashlib, json, pathlib, socket, subprocess, sys, tempfile, zipfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from diagnostics import Recorder, bundle, settings
from interface import Interface
from workshop import Server
from websockets.asyncio.server import serve
import websockets
from ws_client import Client

def port():
    with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]

async def check(binary, package):
    with tempfile.TemporaryDirectory() as tmp:
        root=pathlib.Path(tmp)
        config={'schema':1,'port':port(),'data':str(root/'data'),'runs':str(root/'runs'),'package_root':str(root/'packages'),
                'model':'unchanged.gguf','runtime_pack':'unchanged-pack',
                'diagnostics':{'enabled':True,'level':'trace','include_content':True,'max_file_mb':1,'keep_files':1}}
        for invalid in ({'enabled':1},{'level':'all'},{'keep_files':0},{'unknown':True}):
            try:settings({'diagnostics':invalid});raise AssertionError('invalid settings accepted')
            except ValueError:pass
        cfg=root/'config.json';cfg.write_text(json.dumps(config))
        subprocess.run([sys.executable,str(pathlib.Path(__file__).resolve().parents[1]/'tools/diagnostics.py'),'--config',str(cfg),'enable'],check=True,stdout=subprocess.DEVNULL)
        saved=json.loads(cfg.read_text());assert saved['model']==config['model'] and saved['runtime_pack']==config['runtime_pack']
        trace=root/'runs/logs/diagnostics/runtime.jsonl'
        proc=await asyncio.create_subprocess_exec(binary,'--data',str(root/'data/runtime'),'--package',package,'--port',str(config['port']),
                '--trace-file',str(trace),'--trace-file-mb','1','--trace-backups','1',stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        app=None
        try:
            assert (await asyncio.wait_for(proc.stdout.readline(),10)).startswith(b'READY')
            async with Client(f"ws://127.0.0.1:{config['port']}") as client:
                await client.call('upkeep.watch','upkeep.snapshot')
                await client.call('project.create','project.saved',project_id='home',name='Home',purpose='music')
                await client.call('chat.create','chat.saved',chat_id='chat',project_id='home',title='trace')
                await client.call('chat.send','chat.message',chat_id='chat',text='music trace test')
                while True:
                    e=await client.receive({'chat.message'},timeout=10)
                    if e['data'].get('workspace',{}).get('message',{}).get('role')=='assistant':break
                events=[json.loads(l) for l in trace.read_text().splitlines()]
                assert any(e['kind']=='command.started' and e['payload'].get('type')=='chat.send' for e in events)
                assert any(e['kind']=='runtime.emit' and e['payload'].get('type')=='mesh.impulse' for e in events)
                assert any(e['kind']=='runtime.emit' and e['payload'].get('type')=='chat.message' for e in events)
                # Malformed payload context must be captured, and rotation must be explicit.
                for i in range(130):
                    await client.ws.send(json.dumps({'schema':1,'type':'invalid','request_id':'bad-'+str(i),'text':'x'*8000}))
                    try:await client.receive({'error'},timeout=5)
                    except RuntimeError:pass
                await client.send({'type':'runtime.shutdown','request_id':'stop'})
            await asyncio.wait_for(proc.wait(),10);assert proc.returncode==0,(await proc.stderr.read()).decode()
            assert trace.with_name(trace.name+'.1').is_file()
            assert any(json.loads(l).get('retention_expired') for l in trace.read_text().splitlines())
            uiport=port();app=Interface(config,uiport)
            async with serve(app.handle,'127.0.0.1',uiport,process_request=app.http,max_size=16384,ping_interval=None):
                try:
                    async with websockets.connect(f'ws://127.0.0.1:{uiport}/diagnostics',origin='https://foreign.invalid'):
                        raise AssertionError('foreign telemetry accepted')
                except websockets.exceptions.InvalidStatus:pass
                async with websockets.connect(f'ws://127.0.0.1:{uiport}/diagnostics',origin=f'http://127.0.0.1:{uiport}') as ws:
                    hello=json.loads(await ws.recv());assert hello['settings']['enabled']
                    await ws.send(json.dumps({'kind':'browser.render_failed','payload':{'message':'fixture failure'},'error':True}))
            app.trace.close()
            assert 'fixture failure' in (trace.parent/'interface.jsonl').read_text()
            rec=Recorder(config,'rotation-test')
            for _ in range(12):rec.record('test',{'text':'z'*240000})
            rec.close();assert (trace.parent/'rotation-test.jsonl.1').is_file()
            # Host-only artifacts must never enter an export, even next to logs.
            (root/'runs/logs/model.gguf').write_bytes(b'NOT A LOG')
            (root/'runs/logs/runtime.dll').write_bytes(b'NOT A LOG')
            config['api_key']='private-config-secret'
            result=bundle(config,root/'diagnostics.zip')
            with zipfile.ZipFile(result) as z:
                assert 'runtime/events.jsonl' in z.namelist()
                assert not any(n.endswith(('.gguf','.dll')) for n in z.namelist())
                assert b'private-config-secret' not in z.read('config.json')
                manifest=json.loads(z.read('diagnostics-manifest.json'))
                for f in manifest['files']:assert hashlib.sha256(z.read(f['path'])).hexdigest()==f['sha256']
            config['diagnostics']['include_content']=False
            rec=Recorder(config,'metadata');rec.record('test',{'text':'private-metadata-marker'});rec.close()
            assert 'private-metadata-marker' not in (trace.parent/'metadata.jsonl').read_text()
            with zipfile.ZipFile(bundle(config,root/'metadata.zip')) as z:assert 'runtime/events.jsonl' not in z.namelist()
            # Exercise the real launcher path, including persisted trace flags and process drains.
            launch={**config,'port':port(),'model':'','runtime_pack':'','initial_package':package,
                    'data':str(root/'launcher-data'),'runs':str(root/'launcher-runs')}
            server=Server(pathlib.Path(binary),launch)
            try:await server.start()
            finally:await server.stop()
            assert (root/'launcher-runs/logs/diagnostics/runtime.jsonl').is_file()
            assert 'process.stopped' in (root/'launcher-runs/logs/diagnostics/supervisor.jsonl').read_text()
            disabled={**launch,'diagnostics':{'enabled':False},'runs':str(root/'disabled')}
            rec=Recorder(disabled,'test');rec.record('ignored');rec.close()
            assert not (root/'disabled').exists()
            # Native metadata-only mode likewise omits text, including invalid raw commands.
            metatrace=root/'metadata-native.jsonl';p=port()
            meta=await asyncio.create_subprocess_exec(binary,'--data',str(root/'metadata-chain'),'--package',package,'--port',str(p),
                '--trace-file',str(metatrace),'--trace-content','false',stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
            try:
                assert (await asyncio.wait_for(meta.stdout.readline(),10)).startswith(b'READY')
                async with Client(f'ws://127.0.0.1:{p}') as c:
                    await c.call('project.create','project.saved',project_id='private-project',name='private-native-marker',purpose='music')
                    await c.send({'type':'runtime.shutdown','request_id':'stop'})
                await asyncio.wait_for(meta.wait(),10)
                assert 'private-native-marker' not in metatrace.read_text()
            finally:
                if meta.returncode is None:meta.terminate();await meta.wait()
        finally:
            if proc.returncode is None:proc.terminate();await proc.wait()
    print('native neural/request/error tracing, metadata controls, browser ingestion, rotation and hashed export passed (no UI tests)')
if __name__=='__main__':asyncio.run(check(*sys.argv[1:]))
