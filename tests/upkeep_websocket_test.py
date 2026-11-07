"""Real one-shot timer/restart, quiet idle, worker ticket and root expression acceptance."""
import asyncio,json,pathlib,socket,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from ws_client import Client
from upkeep_worker import run as worker

async def start(directory,port):
    p=await asyncio.create_subprocess_exec(sys.argv[1],'--language-mode','legacy','--data',directory,'--package',sys.argv[2],'--port',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
    line=await asyncio.wait_for(p.stdout.readline(),20)
    assert line.startswith(b'READY'),(line,(await p.stderr.read()).decode())
    return p
async def stop(p,endpoint):
    if p.returncode is None:
        async with Client(endpoint) as c:await c.send({'type':'runtime.shutdown','request_id':'stop'})
        await asyncio.wait_for(p.wait(),20)
    assert p.returncode==0,(await p.stderr.read()).decode()
async def event(client,kind,predicate=lambda e:True):
    async with asyncio.timeout(20):
        while True:
            e=json.loads(await client.ws.recv())
            assert e['type']!='error',e
            if e['type']==kind and predicate(e):return e
async def main():
    with tempfile.TemporaryDirectory() as directory:
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        endpoint=f'ws://127.0.0.1:{port}';p=await start(directory,port)
        try:
            async with Client(endpoint) as c:
                await c.call('root.select','root.selected',root_id='cat',purpose='music composition')
                await c.call('speaker.profile','speaker.profiled',root_id='cat',soul_id='cat',expected_version=0,form='The Last Cat organization',presence=['warm','sisterly'],interests=['music','stories'])
                await c.call('upkeep.configure','upkeep.configured',expected_version=0,settings={'enabled':True,'max_steps':1,'review_delay_ms':20,'upkeep_refractory_ms':20,'max_foundation_calls':4,'max_episodes':4})
                await c.call('review.suggest','review.suggested',root_id='cat',text='music composition review',due_in_ms=1000)
        finally:await stop(p,endpoint)
        p=await start(directory,port)
        fetch_task=None
        try:
            async with Client(endpoint) as c:
                await c.call('upkeep.watch','upkeep.snapshot',root_id='cat')
                result=await event(c,'neuron.completed')
                assert result['data']['status']=='completed',result
                row=result['data']['maintenance']['signals'][0]
                async with Client(endpoint) as rpc:
                    final=(await rpc.call('inspect','wind.snapshot',instance_id=row['instance_id']))['data']
                assert final['speaker']['profile']['form']=='The Last Cat organization'
                assert final['evaluation']['contributions'][0]['expression']['gender_type']=='unspecified'
                assert final['evaluation']['contributions'][0]['expression']['presence']==['warm','sisterly']
                # No reviews due and no command: journal must stay unchanged while idle.
                journal=pathlib.Path(directory)/'events.jsonl';before=journal.read_bytes()
                await asyncio.sleep(.15)
                assert journal.read_bytes()==before,'idle system performed work'
                async with Client(endpoint) as rpc:
                    snapshot=(await rpc.call('upkeep.inspect','upkeep.snapshot'))['data']
                    await rpc.call('upkeep.configure','upkeep.configured',expected_version=snapshot['settings']['version'],settings={'sources':[{'url':'https://example.org/music','topic':'music'}]})
                    await rpc.call('review.suggest','review.suggested',root_id='cat',text='music source review',requires_material=True,due_in_ms=0)
                await event(c,'material.requested')
                # Worker startup recovers the existing ticket; test fetcher never touches the network.
                def fixture(url):return url,'music composition lesson with a recurring motif', 'a'*64
                fetch_task=asyncio.create_task(worker(endpoint,fixture))
                material=await event(c,'material.received');assert material['data']['status']=='completed'
                result=await event(c,'neuron.completed');assert result['data']['status']=='completed'
                async with Client(endpoint) as rpc:
                    snap=(await rpc.call('upkeep.inspect','upkeep.snapshot',root_id='cat'))['data']
                    assert len(snap['materials'])==1 and snap['usage']['fetches']==1
                    assert all(r['status']=='satisfied' for r in snap['reviews'])
                    packages=(await rpc.call('package.list','package.snapshot'))['data']
                    assert not any(e['type']=='wind.activated' for e in map(json.loads,journal.read_text().splitlines()))
                    await rpc.call('upkeep.configure','upkeep.configured',expected_version=snap['settings']['version'],settings={'enabled':False})
                    await rpc.call('review.suggest','review.suggested',root_id='cat',text='music paused review',due_in_ms=0)
                    paused=(await rpc.call('upkeep.inspect','upkeep.snapshot',root_id='cat'))['data']
                    assert paused['pending_signals']==[] and any(r['status']=='scheduled' for r in paused['reviews'])
                chat=await asyncio.create_subprocess_exec(sys.executable,str(pathlib.Path(__file__).resolve().parents[1]/'tools/chat.py'),'--ws',endpoint,'--root','cat',stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
                output,error=await asyncio.wait_for(chat.communicate(b'music conversation\nWhy did you suggest that?\n/reflect Softer music endings please.\n/quit\n'),20)
                assert chat.returncode==0,(output.decode(),error.decode())
                assert b'Feedback saved' in output
                records=[json.loads(line) for line in journal.read_text().splitlines()]
                assert any(e['type']=='wind.checkpoint' and e['data'].get('reply_to') for e in records),'chat lost prior exchange'
                export=pathlib.Path(directory)/'memory.rna'
                process=await asyncio.create_subprocess_exec(sys.executable,str(pathlib.Path(__file__).resolve().parents[1]/'tools/export_rna.py'),str(journal),str(export),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
                out,err=await process.communicate();assert process.returncode==0,(out,err)
                rows=json.loads(export.with_suffix('.index.json').read_text())['rows']
                assert any('reflection-' in row['id'] for row in rows),'RNA export omitted attributed memory'
                assert len(export.read_bytes())==64+256*len(rows)

        finally:
            if fetch_task:fetch_task.cancel();await asyncio.gather(fetch_task,return_exceptions=True)
            await stop(p,endpoint)
    print('timer restart, quiet idle, expression, material worker and paused upkeep passed')
asyncio.run(main())
