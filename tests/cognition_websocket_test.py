import asyncio, json, pathlib, socket, sys, tempfile, uuid
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from ws_client import Client
from mesh_web_worker import run as web_worker

async def main():
    server,parent=sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix='wds-cognitive-wire-') as temp:
        sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
        async def start():
            proc=await asyncio.create_subprocess_exec(server,'--language-mode','legacy','--data',temp,'--package',parent,'--port',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
            assert b'READY' in await asyncio.wait_for(proc.stdout.readline(),20)
            return proc
        proc=await start();worker=None
        try:
            async with Client(f'ws://127.0.0.1:{port}') as c:
                await c.call('upkeep.watch','upkeep.snapshot')
                await c.call('project.create','project.saved',project_id='home',name='Home',purpose='learn door operations')
                await c.call('chat.create','chat.saved',project_id='home',chat_id='native',title='Cognitive test')
                await c.call('chat.update','chat.saved',chat_id='native',expected_version=1,mode='host')
                async def chat(text):
                    await c.call('chat.send','chat.message',chat_id='native',text=text)
                    while True:
                        e=await c.receive({'chat.message'},timeout=20)
                        if e['data']['workspace']['message']['chat_id']=='native' and e['data']['workspace']['message']['role']=='assistant':
                            m=e['data']['workspace']['message'];assert m['status']!='failed',m;return m
                assert 'Hello' in (await chat('hello'))['text']
                await chat('teach unlock door : locked=true,key=true,color=red => locked=false')
                await chat('teach unlock door : locked=true,key=true,color=blue => locked=false')
                await chat('entity amber-gate door : locked=true,key=true,color=amber')
                assert 'I predict unlock' in (await chat('predict unlock amber-gate'))['text']
                snapshot=(await c.call('workspace.snapshot','workspace.snapshot'))['data']
                current=next(x for x in snapshot['chats'] if x['id']=='native')
                await c.call('chat.update','chat.saved',chat_id='native',expected_version=current['version'],mode='wind')
                assert 'I predict unlock' in (await chat('predict unlock amber-gate'))['text']
                # Missing host knowledge wakes one background web/archive review.
                mesh=(await c.call('mesh.inspect','mesh.snapshot',summary=True))['data']
                await c.call('mesh.configure','mesh.configured',expected_version=mesh['settings']['version'],settings={'web_enabled':True})
                snapshot=(await c.call('workspace.snapshot','workspace.snapshot'))['data']
                current=next(x for x in snapshot['chats'] if x['id']=='native')
                await c.call('chat.update','chat.saved',chat_id='native',expected_version=current['version'],mode='host')
                fetched=[]
                def search(query):
                    fetched.append(query)
                    return [{'url':'https://en.wikipedia.org/wiki/Resonance','title':'Resonance',
                             'text':'Chromatic resonance is the fixture source description for this test, not a scientific claim.','excerpt':True}]
                worker=asyncio.create_task(web_worker(f'ws://127.0.0.1:{port}',search))
                await chat('What is chromatic resonance?')
                while True:
                    event=await c.receive({'chat.message'},timeout=20)
                    message=event['data']['workspace']['message']
                    if message['role']=='assistant' and message.get('kind')=='contribution':break
                assert len(fetched)==1 and 'fixture source description' in message['text'],message
                worker.cancel();await asyncio.gather(worker,return_exceptions=True);worker=None
                before=(await c.call('cognition.inspect' ,'cognition.snapshot',root_id='home'))['data']
                active=(await c.call('package.list','package.snapshot'))['data']['active']
                assert all(v==active['general'] for v in active.values())
                await c.send({'type':'runtime.shutdown','request_id':uuid.uuid4().hex})
            await asyncio.wait_for(proc.wait(),10)
            # Inspect journal for explicit no-generation instrumentation and trace links.
            events=[json.loads(x) for x in (pathlib.Path(temp)/'events.jsonl').read_text().splitlines()]
            cognitive=[e for e in events if e['type']=='cognition.committed']
            assert len(cognitive)>=6
            assert all(e['data']['trace']['generation_calls']==0 and e['data']['trace']['encoder_calls']==0 for e in cognitive)
            assert not any(e['type']=='token' for e in events)
            proc=await start()
            async with Client(f'ws://127.0.0.1:{port}') as c:
                after=(await c.call('cognition.inspect','cognition.snapshot',root_id='home'))['data']
                assert after==before,'restart changed cognitive knowledge'
                await c.send({'type':'runtime.shutdown','request_id':uuid.uuid4().hex})
            await asyncio.wait_for(proc.wait(),10)
        finally:
            if worker:worker.cancel();await asyncio.gather(worker,return_exceptions=True)
            if proc.returncode is None:proc.terminate();await proc.wait()
        print('Host and neural chat, held-out transfer, no generation, explicit package activation and restart passed.')
asyncio.run(main())
