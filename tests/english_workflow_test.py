"""Native sequence trainer -> registered candidate -> explicit activation, via real WS."""
import asyncio,json,pathlib,socket,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from ws_client import Client
from english_workshop import run
from english_review import review
ROOT=pathlib.Path(__file__).resolve().parents[1]
async def test(binary,package):
    with tempfile.TemporaryDirectory() as directory:
        folder=pathlib.Path(directory)
        with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
        curriculum={'schema':'english.conversations/1','conversations':[]}
        for split in ('train','validation','test'):
            for i in range(3):curriculum['conversations'].append({'id':f'{split}-{i}','group':split,'split':split,'turns':[{'role':'user','text':f'{split} example {i}'},{'role':'assistant','text':'A small example.'}]})
        data=folder/'corpus.json';data.write_text(json.dumps(curriculum))
        config={'port':port,'build_dir':str(pathlib.Path(binary).resolve().parent),'initial_package':str(pathlib.Path(package).resolve()),'package_root':str(folder/'packages'),'runs':str(folder/'runs')}
        proc=await asyncio.create_subprocess_exec(binary,'--port',str(port),'--data',str(folder/'arc'),'--package',package,'--packages',config['package_root'],stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        try:
            assert (await proc.stdout.readline()).startswith(b'READY')
            async with Client(f'ws://127.0.0.1:{port}') as client:
                assert client.hello['language_mode']=='strict', 'default silently enabled a teacher or legacy keyword language path'
                before=(await client.call('package.list','package.snapshot'))['data']['active']['language']
                result=await run(config,str(data),2)
                after=(await client.call('package.list','package.snapshot'))['data']['active']['language']
                assert before==after and result['activated'] is False
                candidate=pathlib.Path(result['package']);assert (candidate/'policy.wind').stat().st_size==1152
                assert (candidate/'language.tensor').is_file() and (candidate/'weight-updates.jsonl').stat().st_size>0
                await client.call('wind.activate','wind.activated',domain='language',package_key=result['package_key'],expected_active_key=before)
                assert (await client.call('package.list','package.snapshot'))['data']['active']['language']==result['package_key']
                await client.send({'type':'runtime.shutdown','request_id':'done'})
            await asyncio.wait_for(proc.wait(),10)
        finally:
            if proc.returncode is None:proc.terminate();await proc.wait()
        source={'lessons':[{'root_id':'home','source_instance':'one','teacher_model':'fixture','context':[],'input':'a reviewed message','response':'a reviewed answer'}]}
        merged=review(source,curriculum,['one']);assert len(merged['conversations'])==10 and merged['conversations'][-1]['split']=='train'
        assert [r for r in merged['conversations'] if r['split']=='test']==[r for r in curriculum['conversations'] if r['split']=='test']
        source['lessons'][0]['input']='test example 0'
        try:review(source,curriculum,['one']);raise AssertionError('held-out input admitted to training')
        except ValueError:pass
    print('Native English training, WS registration, no auto activation, explicit activation, review import and held-out protection passed')
if __name__=='__main__':asyncio.run(test(*sys.argv[1:]))
