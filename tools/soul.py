#!/usr/bin/env python3
"""Root expressions, attributed reflections and versioned commands over WebSocket."""
import argparse,asyncio,json,pathlib,sys,uuid
from ws_client import Client

async def main(args):
    async with Client(args.ws) as client:
        if args.command=='watch':
            event=await client.call('upkeep.watch','upkeep.snapshot',root_id=args.root)
            print(json.dumps(event,ensure_ascii=False),flush=True)
            async for message in client.ws:print(message,flush=True)
            return
        if args.command=='inspect':
            print(json.dumps(await client.call('upkeep.inspect','upkeep.snapshot',root_id=args.root),indent=2));return
        if args.command=='reflect':
            values={'root_id':args.root,'actor_soul_id':args.actor,'actor_kind':args.kind,'text':args.text}
            if args.soul:values['soul_id']=args.soul
            if args.instance:values['instance_id']=args.instance
            print(json.dumps(await client.call('reflection.record','reflection.recorded',**values),indent=2));return
        message=json.loads(pathlib.Path(args.file).read_text())
        trace=message.setdefault('request_id',uuid.uuid4().hex)
        await client.send(message)
        while True:
            event=json.loads(await asyncio.wait_for(client.ws.recv(),60))
            if event.get('type')=='error':raise RuntimeError(event)
            if event.get('trace_id')==trace:
                print(json.dumps(event,indent=2));return

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ws',default='ws://127.0.0.1:8765')
    sub=p.add_subparsers(dest='command',required=True)
    for name in ('inspect','watch'):
        s=sub.add_parser(name);s.add_argument('--root',default='*')
    s=sub.add_parser('send');s.add_argument('file')
    s=sub.add_parser('reflect');s.add_argument('--root',required=True);s.add_argument('--actor',required=True)
    s.add_argument('--kind',choices=['human','voice'],default='human');s.add_argument('--soul');s.add_argument('--instance');s.add_argument('--text',required=True)
    try:asyncio.run(main(p.parse_args()))
    except KeyboardInterrupt:pass
    except Exception as error:print(str(error) or type(error).__name__,file=sys.stderr);sys.exit(1)
