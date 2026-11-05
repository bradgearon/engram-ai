"""Native optimizer, resumable checkpoint, ABI and explicit activation integration."""
import asyncio,json,pathlib,shutil,socket,struct,subprocess,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from ws_client import Client

def read(p):return json.loads(p.read_text())
def run(trainer,job):
    p=subprocess.run([trainer,'--job',str(job)],capture_output=True,text=True)
    assert p.returncode==0,p.stderr
    return [json.loads(x) for x in p.stdout.splitlines()]

def training(trainer,parent,root):
    caps=json.loads(subprocess.check_output([trainer,'--capabilities']))
    rows=[{'features':[float(i==j) for i in range(16)],'target':j%7} for _ in range(40) for j in range(7)]
    policies=[]
    for backend in caps['backends']:
        output=root/backend;job={'schema':1,'domain':'music','parent':parent,'output':str(output),
            'train':rows,'validation':rows[:7],'epochs':20,'learning_rate':.05,'backend':backend}
        path=root/(backend+'.json');path.write_text(json.dumps(job))
        process=subprocess.Popen([trainer,'--job',str(path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        first=json.loads(process.stdout.readline());assert first['type']=='training.progress',first
        # Capture an actual published epoch boundary, then replay that checkpoint later.
        cp=pathlib.Path(str(output)+'.checkpoint.json');saved=cp.read_bytes()
        out,err=process.communicate(timeout=60);assert process.returncode==0,err
        policy=(output/'policy.wind').read_bytes();manifest=read(output/'manifest.json')
        assert len(policy)==1152 and policy[:8]==b'WDSWND1\0'
        assert struct.unpack_from('<IIII',policy,8)==(1,16,64,272)
        assert all(-4<=x<=4 for x in struct.unpack_from('<272f',policy,64))
        assert manifest['maturity']=='seeded' and manifest['schema']==1
        assert manifest['training']['best_epoch']>0
        shutil.rmtree(output);cp.write_bytes(saved)
        run(trainer,path)
        assert (output/'policy.wind').read_bytes()==policy,'resume changed resulting weights'
        assert read(output/'manifest.json')==manifest,'resume changed candidate manifest'
        run(trainer,path)  # Same immutable job/package is idempotent.
        policies.append(struct.unpack_from('<272f',policy,64))
        bad={**job,'train':[{'features':[2.0]*16,'target':0}]*2};badpath=root/'invalid.json';badpath.write_text(json.dumps(bad))
        rejected=subprocess.run([trainer,'--job',str(badpath)],capture_output=True)
        assert rejected.returncode!=0 and (output/'policy.wind').read_bytes()==policy
    if len(policies)>1:
        assert max(abs(a-b) for a,b in zip(*policies))<.002,'scalar/ggml SGD diverged'
    return root/caps['default_backend']

async def integration(server,parent,root,candidate):
    with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
    endpoint=f'ws://127.0.0.1:{port}';selected=None;request='native-test-episode'
    task={'schema':1,'domain':'music','id':'range','artifact':{'notes':[{'pitch':48,'duration':1}]*4},
        'requirements':{'pitch_min':60,'pitch_max':84,'target_ticks':4,'home_pc':0}}
    for attempt in range(2):
        process=await asyncio.create_subprocess_exec(server,'--data',str(root/'data'),'--packages',str(root),
            '--package',parent,'--port',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        try:
            assert b'READY' in await asyncio.wait_for(process.stdout.readline(),20)
            async with Client(endpoint) as client:
                active=(await client.call('package.list','package.snapshot'))['data']['active']
                if attempt==0:
                    await client.call('root.select','root.selected',root_id='practice',purpose='Music practice')
                    topology=(await client.call('heart.inspect','heart.snapshot',root_id='practice'))['data']['mesh']
                    assert not topology['growth_roles']['crest_cell']['writes_garden']
                    assert not topology['growth_roles']['stem_cell']['writes_garden']
                    manifest=read(candidate/'manifest.json')
                    registered=await client.call('package.register','package.available',path=candidate.name,checksum=manifest['checksum_fnv1a64'])
                    selected=registered['data']['key']
                    assert (await client.call('package.list','package.snapshot'))['data']['active']==active
                    result=await client.episode(request,root_id='practice',text='repair range',task=task,package_key=selected,max_steps=2)
                    assert result['status'] in ('completed','exhausted')
                    assert result['evaluation']['evaluator']=='wds.ai.heart.music/1'
                    assert (await client.call('package.list','package.snapshot'))['data']['active']==active
                    await client.call('wind.activate','wind.activated',request='activate-test',domain='music',package_key=selected,expected_active_key=active['music'])
                else:
                    assert active['music']==selected
                    snapshot=await client.episode(request)
                    assert snapshot['package_key']==selected and snapshot['status'] in ('completed','exhausted')
                    assert snapshot['evaluator']==snapshot['evaluation']['evaluator']
                await client.send({'type':'runtime.shutdown','request_id':'stop'})
            await asyncio.wait_for(process.wait(),20)
            assert process.returncode==0,(await process.stderr.read()).decode()
        finally:
            if process.returncode is None:process.terminate();await process.wait()
    journal=[json.loads(x) for x in (root/'data/events.jsonl').read_text().splitlines()]
    assert sum(x['type']=='wind.activated' for x in journal)==1

if __name__=='__main__':
    with tempfile.TemporaryDirectory() as tmp:
        root=pathlib.Path(tmp);candidate=training(sys.argv[1],sys.argv[3],root)
        asyncio.run(integration(sys.argv[2],sys.argv[3],root,candidate))
    print('Native training/resume, ABI, bounded inputs, candidate isolation, activation and replay passed.')
