#!/usr/bin/env python3
"""Bounded local acceptance fixture; no GGUF or held-out curriculum tuning."""
import argparse,asyncio,json,pathlib,socket,tempfile,uuid
from ws_client import Client
ROOT=pathlib.Path(__file__).resolve().parents[1]
async def main(args):
 with tempfile.TemporaryDirectory() as directory:
  with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
  endpoint=f'ws://127.0.0.1:{port}'
  process=await asyncio.create_subprocess_exec(str(pathlib.Path(args.binary).resolve()),'--data',directory,'--package',str(ROOT/'packages/breeze'),'--packages',str(ROOT/'packages'),'--port',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
  assert (await process.stdout.readline()).startswith(b'READY')
  try:
   async with Client(endpoint) as c:
    await c.call('root.select','root.selected',root_id='selective',purpose='music composition')
    await c.call('upkeep.watch','upkeep.snapshot',root_id='selective')
    artifacts=[];reused=0
    for _ in range(10):
     data=(await c.call('neuron.signal','neuron.decision',root_id='selective',text='music composition plan',max_steps=1))['data']
     if data['status']=='reuse':reused+=1;artifacts.append(data['artifact'])
     else:
      while True:
       event=json.loads(await asyncio.wait_for(c.ws.recv(),20))
       if event['type']=='error':raise RuntimeError(event)
       if event['type']=='neuron.completed' and event['data']['signal_key']==data['signal_key']:
        assert event['data']['status']=='completed';artifacts.append(event['data']['artifact']);break
    assert reused==9 and len(set(artifacts))==1
    for _ in range(10):
     trace=uuid.uuid4().hex
     await c.send({'type':'seed','request_id':trace,'root_id':'direct','text':'music composition plan','max_steps':1})
     assert (await c.receive({'episode.finished'},trace))['data']['status']=='completed'
    manifest=json.loads((ROOT/args.candidate/'manifest.json').read_text())
    available=(await c.call('package.register','package.available',path=str(pathlib.Path(args.candidate).relative_to('packages')),checksum=manifest['checksum_fnv1a64']))['data']['key']
    snapshot=(await c.call('package.list','package.snapshot'))['data']
    await c.call('wind.activate','wind.activated',domain='music',package_key=available,expected_active_key=snapshot['active']['music'])
    await c.call('root.select','root.selected',root_id='practice',purpose='music composition practice')
    await c.call('upkeep.watch','upkeep.snapshot',root_id='practice')
    await c.call('upkeep.configure','upkeep.configured',expected_version=0,settings={'enabled':True,'max_steps':2,'review_delay_ms':30,'upkeep_refractory_ms':30})
    task={'schema':1,'id':'upkeep-acceptance','domain':'music','artifact':{'notes':[{'pitch':p,'duration':1} for p in [36,38,40,38]]},'requirements':{'pitch_min':60,'pitch_max':84,'target_ticks':8,'home_pc':0,'return_home':True}}
    trace=uuid.uuid4().hex
    await c.send({'type':'task.run','request_id':trace,'root_id':'practice','text':'music composition targeted motif practice','max_steps':1,'task':task})
    initial=(await c.receive({'episode.finished'},trace))['data'];assert initial['status']=='exhausted'
    while True:
     event=json.loads(await asyncio.wait_for(c.ws.recv(),20))
     if event['type']=='error':raise RuntimeError(event)
     if event['type']=='neuron.completed' and event['root_id']=='practice':
      result=event['data'];break
    final=(await c.call('upkeep.inspect','upkeep.snapshot',root_id='practice'))['data']
    report={'schema':1,'fixture':'deterministic demo and native symbolic music; no language-model/GPU performance claim',
     'repeated_requests':10,'selective_reused':reused,'identical_cached_artifact':len(set(artifacts))==1,
     'practice':{'initial_status':initial['status'],'initial_score':initial['evaluation']['score'],'review_status':result['status'],'review_score':result['evaluation']['score'],'review_records':final['reviews'],'flesh_after':final['flesh'],'usage':final['usage']}}
  finally:
   async with Client(endpoint) as c:await c.send({'type':'runtime.shutdown','request_id':'stop'})
   await asyncio.wait_for(process.wait(),20)
   assert process.returncode==0,(await process.stderr.read()).decode()
  events=[json.loads(line) for line in (pathlib.Path(directory)/'events.jsonl').read_text().splitlines()]
  counts={root:sum(e['type']=='heart.evaluated' and e['root_id']==root for e in events) for root in ('direct','selective')}
  assert counts=={'direct':10,'selective':1}
  activations=[e for e in events if e['type']=='wind.activated'];assert len(activations)==1
  report.update(episode_evaluations=counts,activation_events=1,activation_origin='explicit test operator before practice')
  output=pathlib.Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--binary',default='build/wds-ai');p.add_argument('--candidate',default='packages/candidates/music/cefed33bfd36b669120438a288036533907b1f92');p.add_argument('--output',default='examples/verification-v04/upkeep.json');asyncio.run(main(p.parse_args()))
