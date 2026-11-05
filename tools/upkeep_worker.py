"""One configured-source fetch at a time, driven by WebSocket material tickets."""
import asyncio,json,uuid
from ws_client import Client
from learn import fetch

def bounded_chunks(text, limit):
    chunks=[];part='';limited=False
    for word in text.split():
        if len(word.encode())>1500:
            limited=True;continue
        candidate=(part+' '+word).strip()
        if len(candidate.encode())>1500:
            chunks.append(part);part=word
            if len(chunks)==limit:
                limited=True;part='';break
        else:part=candidate
    if part:chunks.append(part)
    return chunks,limited

async def complete(endpoint,item,fetcher=fetch):
    async with Client(endpoint) as rpc:
        claim=uuid.uuid4().hex
        try:
            assigned=await rpc.call('material.claim','material.assigned',request=claim,
                root_id=item['root_id'],material_key=item['key'])
        except RuntimeError as error:
            print(json.dumps({'type':'material.claim_rejected','key':item['key'],'reason':str(error)}),flush=True)
            return
        item=assigned['data']['request']
        try:
            final_url,text,digest=await asyncio.wait_for(asyncio.to_thread(fetcher,item['url']),100)
            chunks,limited=bounded_chunks(text,item['max_chunks'])
            if not chunks:raise ValueError('source contains no usable text within the extraction bounds')
            payload={'chunks':chunks,'content_sha256':digest,'extraction_limited':limited,'resolved_url':final_url}
        except Exception as error:
            payload={'error':(str(error) or type(error).__name__)[:1024]}
        result=await rpc.call('material.complete','material.received',root_id=item['root_id'],
            material_key=item['key'],claim_id=claim,**payload)
        print(json.dumps({'type':'worker.material_result','key':item['key'],
            'status':result['data']['status'],'extraction_limited':payload.get('extraction_limited',False),
            'reason':payload.get('error','')}),flush=True)

async def run(endpoint,fetcher=fetch):
    async with Client(endpoint) as watch:
        snapshot=(await watch.call('upkeep.watch','upkeep.snapshot',root_id='*'))['data']
        pending=[m for m in snapshot['materials'] if m['status']=='pending'] if snapshot['settings']['enabled'] else []
        while True:
            for item in pending:await complete(endpoint,item,fetcher)
            pending=[]
            event=json.loads(await watch.ws.recv()) # No polling or idle timer.
            if event['type']=='error':raise RuntimeError(event)
            if event['type']=='material.requested':pending.append(event['data']['request'])
            elif event['type']=='upkeep.configured' and event['data']['settings']['enabled']:
                async with Client(endpoint) as rpc:
                    snapshot=(await rpc.call('upkeep.inspect','upkeep.snapshot'))['data']
                    pending=[m for m in snapshot['materials'] if m['status']=='pending']
