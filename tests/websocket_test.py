import asyncio,json,pathlib,sys,tempfile,time,uuid,socket
import websockets
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from learn import ingest,train,fetch
from teacher_bridge import run as teach
from types import SimpleNamespace

async def recv_type(ws,kind):
 while True:
  e=json.loads(await asyncio.wait_for(ws.recv(),15))
  if e.get('type')=='error' and kind!='error':raise AssertionError(e)
  if e.get('type')==kind:return e
async def main():
 with tempfile.TemporaryDirectory() as tmp:
  with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
  process=await asyncio.create_subprocess_exec(sys.argv[1],'--language-mode','legacy','--data',tmp+'/data','--package',sys.argv[2],'--port',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
  try:
   ready=await asyncio.wait_for(process.stdout.readline(),15);assert b'READY' in ready,ready
   endpoint=f'ws://127.0.0.1:{port}'
   async with websockets.connect(endpoint,ping_interval=None) as ws:
    await recv_type(ws,'hello')
    await ws.send(json.dumps({'schema':1,'type':'root.select','request_id':'root','root_id':'garden','purpose':'testing'}));await recv_type(ws,'root.selected')
    packet={'schema':1,'intent':'memory.upsert','hops':0,'root_id':'garden','engram_id':'fact','packet_id':'p1','expected_version':0,'expires_ms':int(time.time()*1000)+60000,'text':'The silver cat plays a violet violin.','source':'integration fixture','claim_status':'observed'}
    await ws.send(json.dumps({'schema':1,'type':'memory.upsert','request_id':'p1','packet':packet}));await recv_type(ws,'memory.accepted')
    await ws.send(json.dumps({'schema':1,'type':'memory.upsert','request_id':'dupe','packet':packet}));assert 'duplicate' in (await recv_type(ws,'error'))['message']
    await ws.send(json.dumps({'schema':1,'type':'seed','request_id':'seed1','root_id':'garden','text':'Describe the silver cat.','max_steps':2}))
    final=await recv_type(ws,'episode.finished');assert 'Lumen of garden' not in final['data']['artifact'];assert 'violet violin' in final['data']['artifact'];assert final['data']['status']=='completed'
    packet.update(packet_id='p2',expected_version=1,text='The silver cat now plays a green flute.')
    await ws.send(json.dumps({'schema':1,'type':'memory.upsert','request_id':'p2','packet':packet}));assert (await recv_type(ws,'memory.accepted'))['data']['version']==2
    await ws.send(json.dumps({'schema':1,'type':'seed','request_id':'seed2','root_id':'garden','text':'Describe the silver cat.','max_steps':1}))
    final=await recv_type(ws,'episode.finished');assert 'green flute' in final['data']['artifact'];assert 'violet violin' not in final['data']['artifact']
    await ws.send('{bad');await recv_type(ws,'error')
   lesson=' '.join(f'Example {i}: harmonic motion uses intervals and contrast. '+('melody rhythm chords phrasing. '*45) for i in range(20))
   report=await ingest(endpoint,'garden',lesson,'test-source',sys.argv[2],tmp+'/candidate')
   assert report['status']=='memory_only',report
   assert not pathlib.Path(tmp+'/candidate/policy.wind').exists()
   async def teacher(ws):
    await ws.send(json.dumps({'schema':1,'type':'hello','foundation':'fixture-teacher/1'}))
    request=json.loads(await ws.recv());assert request['type']=='seed' and 'Topic:' in request['text']
    await ws.send(json.dumps({'schema':1,'type':'episode.finished','data':{'artifact':'A source-linked lesson about musical contrast and motif variation.'}}))
   async with websockets.serve(teacher,'127.0.0.1',0,ping_interval=None) as ts:
    teacher_port=ts.sockets[0].getsockname()[1]
    await teach(SimpleNamespace(teacher=f'ws://127.0.0.1:{teacher_port}',student=endpoint,root='garden',topic=None,base=sys.argv[2],output=tmp+'/teacher-candidate',allow_demo=False))
   for url in ('http://example.com','https://127.0.0.1','https://[::1]'):
    try:fetch(url)
    except ValueError:pass
    else:raise AssertionError('unsafe URL accepted')
  finally:
   process.terminate();await asyncio.wait_for(process.wait(),20)
   stderr=(await process.stderr.read()).decode()
   if sys.platform!='win32':assert process.returncode==0,(process.returncode,stderr)
   elif pathlib.Path(tmp+'/data/writer.lock').exists():pathlib.Path(tmp+'/data/writer.lock').rmdir()
  events=[json.loads(s) for s in pathlib.Path(tmp+'/data/events.jsonl').read_text().splitlines()]
  assert any(e['type']=='learning.candidate' for e in events)
  assert all(e['seq']==i+1 for i,e in enumerate(events))
  tool=pathlib.Path(__file__).resolve().parents[1]/'tools/export_rna.py'
  exporter=await asyncio.create_subprocess_exec(sys.executable,str(tool),tmp+'/data/events.jsonl',tmp+'/memory.rna',stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
  out,err=await exporter.communicate();assert exporter.returncode==0,(out,err)
  assert (pathlib.Path(tmp+'/memory.rna').stat().st_size-64)%256==0

  # Exact restart/replay; no idle resumption or polling.
  process=await asyncio.create_subprocess_exec(sys.argv[1],'--language-mode','legacy','--data',tmp+'/data','--package',sys.argv[2],'--port',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
  assert b'READY' in await asyncio.wait_for(process.stdout.readline(),15)
  process.terminate();await asyncio.wait_for(process.wait(),20)
  if sys.platform!='win32':assert process.returncode==0
 print('websocket, live memory, learning, URL guard and replay tests passed')
asyncio.run(main())
