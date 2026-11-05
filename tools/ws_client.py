"""Small sequential WebSocket client. Await pushed events; never poll."""
import asyncio,json,uuid
import websockets

class Client:
    def __init__(self,endpoint):self.endpoint=endpoint;self.ws=None
    async def __aenter__(self):
        self.ws=await websockets.connect(self.endpoint,ping_interval=None,origin=None,max_size=262144,max_queue=64,close_timeout=2)
        self.hello=json.loads(await asyncio.wait_for(self.ws.recv(),30));return self
    async def __aexit__(self,*args):await self.ws.close()
    async def send(self,message):await self.ws.send(json.dumps({'schema':1,**message},ensure_ascii=False))
    async def receive(self,types,trace=None,timeout=300):
        while True:
            try:event=json.loads(await asyncio.wait_for(self.ws.recv(),timeout))
            except asyncio.TimeoutError:raise RuntimeError('WebSocket wait timed out; work may still be running. Reconnect with episode.attach or upkeep.inspect; no work was silently discarded.')
            if event.get('type')=='error':raise RuntimeError(event)
            if event.get('type') in types and (trace is None or event.get('trace_id')==trace):return event
    async def call(self,kind,expected,request=None,**values):
        request=request or uuid.uuid4().hex
        await self.send({'type':kind,'request_id':request,**values})
        return await self.receive({expected},request)
    async def episode(self,request,**values):
        attach=uuid.uuid4().hex
        await self.send({'type':'episode.attach','request_id':attach,'episode_request_id':request})
        response=await self.receive({'episode.attached','episode.missing'},attach)
        if response['type']=='episode.missing':
            await self.send({'type':'task.run','request_id':request,**values})
        else:
            snapshot=response['data']
            if snapshot['status'] in ('completed','exhausted','failed','cancelled'):return snapshot
            if snapshot['status']=='paused':
                await self.send({'type':'resume','request_id':uuid.uuid4().hex,'instance_id':snapshot['id']})
        while True:
            event=await self.receive({'episode.finished','wind.checkpoint'},request)
            if event['type']=='episode.finished':return event['data']
            if event['data']['status'] in ('failed','cancelled'):return event['data']
