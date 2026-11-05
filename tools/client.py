#!/usr/bin/env python3
import argparse,asyncio,json,uuid
import websockets
async def run(a):
 async with websockets.connect(a.ws,ping_interval=None,max_size=262144,origin=None) as ws:
  await ws.send(json.dumps({'schema':1,'type':'seed','request_id':uuid.uuid4().hex,'root_id':a.root,'text':a.seed,'max_steps':3,'evaluator':a.evaluator,'voice_budget':a.voice_budget}))
  async for raw in ws:
   e=json.loads(raw)
   if e.get('type')=='token':print(e['text'],end='',flush=True)
   else:print('\n'+json.dumps(e))
   if e.get('type') in ('episode.finished','error') or (e.get('type')=='wind.checkpoint' and e['data']['status']=='failed'):break
p=argparse.ArgumentParser();p.add_argument('seed');p.add_argument('--root',default='workshop');p.add_argument('--ws',default='ws://127.0.0.1:8765');p.add_argument('--evaluator',default='wds.ai.heart.composite/1');p.add_argument('--voice-budget',type=int,default=4);asyncio.run(run(p.parse_args()))
