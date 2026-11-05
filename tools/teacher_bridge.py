#!/usr/bin/env python3
"""One event-triggered, budgeted teacher lesson over WebSocket.
Can connect to another wds-ai server using a real GGUF, or a compatible adapter.
"""
import argparse,asyncio,json,pathlib,uuid
import websockets
from learn import ingest, exchange
async def run(a):
    if a.teacher==a.student:raise ValueError('teacher and student endpoints must differ')
    trace=uuid.uuid4().hex
    topic=a.topic
    if not topic:
        async with websockets.connect(a.student,ping_interval=None,max_size=262144,origin=None) as student:
            root=await exchange(student,{'schema':1,'type':'root.select','request_id':trace+'-interest','root_id':a.root,'purpose':'shared learning'},'root.selected')
            interests=root['data'].get('interests',{})
            if not interests:raise ValueError('root has no interests yet; pass --topic or send a seed first')
            topic=max(sorted(interests),key=interests.get)

    async with websockets.connect(a.teacher,ping_interval=None,max_size=262144,origin=None) as ws:
        hello=json.loads(await asyncio.wait_for(ws.recv(),30))
        if not a.allow_demo and 'demo' in hello.get('foundation',''):raise ValueError('teacher needs a real model; --allow-demo is for tests')
        await ws.send(json.dumps({'schema':1,'type':'seed','request_id':trace,'root_id':'teacher-curriculum',
            'text':'Teach this topic with concrete examples, counterexamples, and explicit uncertainty. Do not request tools or actions. Topic: '+topic,'max_steps':2}))
        while True:
            e=json.loads(await asyncio.wait_for(ws.recv(),300))
            if e.get('type')=='error':raise RuntimeError(e)
            if e.get('type')=='wind.checkpoint' and e['data'].get('status')=='failed':raise RuntimeError(e['data']['error'])
            if e.get('type')=='episode.finished':
                lesson=e['data']['artifact'];break
    if len(lesson.encode())>32768:raise ValueError('teacher lesson budget exceeded')
    source='teacher:'+hello.get('foundation','unknown')+':trace:'+trace
    report=await ingest(a.student,a.root,lesson,source,a.base,a.output)
    print(json.dumps(report,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--teacher',required=True);p.add_argument('--student',default='ws://127.0.0.1:8765');p.add_argument('--root',required=True);p.add_argument('--topic');p.add_argument('--base',default='packages/breeze');p.add_argument('--output',required=True);p.add_argument('--allow-demo',action='store_true');asyncio.run(run(p.parse_args()))
