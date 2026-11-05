#!/usr/bin/env python3
"""Interactive text conversation with scoped souls and attributed astroengram feedback."""
import argparse,asyncio,json,sys,uuid
from ws_client import Client

async def chat(args):
    async with Client(args.ws) as c:
        print('Foundation:',c.hello['foundation'])
        await c.call('root.select','root.selected',root_id=args.root,purpose=args.purpose)
        scope=(await c.call('heart.inspect','heart.snapshot',root_id=args.root))['data']
        if args.user==args.root:raise ValueError('--user must be distinct from the root identity')
        if args.user not in {s['soul_id'] for s in scope['souls']}:
            await c.call('relationship.declare','relationship.declared',root_id=args.root,soul_id=args.user,
                soul_name=args.name,purpose='The user providing goals, preferences and feedback',relationship_to=args.root,
                relationship='human conversation partner',voice_id='purpose',expected_version=0,paused=True)
        await c.call('upkeep.watch','upkeep.snapshot',root_id=args.root)
        print('Text chat ready. /soul ID or /soul auto; /reflect TEXT; /profile; /quit.\nUser turns are recorded as attributed memory in this root.')
        target=args.soul;previous=None
        while True:
            try:line=(await asyncio.to_thread(input,'You> ')).strip()
            except EOFError:return
            if not line:continue
            if line=='/quit':return
            if line.startswith('/soul '):
                requested=line[6:].strip()
                if requested!='auto':await c.call('route.resolve','route.selected',root_id=args.root,soul_id=requested,text='conversation')
                target=None if requested=='auto' else requested
                print('Speaker preference:',target or 'automatic');continue
            if line=='/profile':
                values={'root_id':args.root,'text':'conversation'}
                if target:values['soul_id']=target
                print(json.dumps((await c.call('route.resolve','route.selected',**values))['data'],indent=2,ensure_ascii=False));continue
            reflection=line[9:].strip() if line.startswith('/reflect ') else line
            if len(reflection.encode())>2048:
                print('Message exceeds 2048 UTF-8 bytes; split it into separate turns. Nothing was accepted.');continue
            values={'root_id':args.root,'actor_soul_id':args.user,'actor_kind':'human','text':reflection}
            if target:values['soul_id']=target
            if previous:values['instance_id']=previous
            await c.call('reflection.record','reflection.recorded',**values)
            if line.startswith('/reflect '):print('Feedback saved for the next attempt.');continue
            values={'root_id':args.root,'text':line,'max_steps':1,'max_output_tokens':args.tokens}
            if previous:values['reply_to']=previous
            if target:values['soul_id']=target
            decision=(await c.call('neuron.signal','neuron.decision',**values))['data']
            if decision['status'] in ('hold','reuse'):
                print(decision.get('artifact',decision['reason']));continue
            key=decision['signal_key'];instance=None;streamed=False;profile=None
            try:
                async with asyncio.timeout(args.timeout):
                    while True:
                        event=json.loads(await c.ws.recv());data=event.get('data',{})
                        if event['type']=='error':raise RuntimeError(event)
                        if event['type']=='neuron.dispatched' and data['signal_key']==key:instance=data['instance_id']
                        if event['type']=='token' and event.get('instance_id')==instance:
                            if not streamed:
                                profile=event.get('speaker',{});print('Speaker:',profile.get('selected_soul','root'));print('AI> ',end='',flush=True)
                            print(event['text'],end='',flush=True);streamed=True
                        if event['type']=='wind.checkpoint' and data.get('id')==instance and data['status']=='failed':
                            print('\nResponse incomplete:',data.get('error'));print('Saved partial output:',data.get('partial_artifact',''));previous=instance;break
                        if event['type'] in ('neuron.completed','neuron.failed') and data.get('signal_key')==key:
                            if not streamed:print('AI>',data.get('artifact',''))
                            else:print()
                            if data.get('status') not in ('completed','reuse'):print('Outcome:',data.get('status','failed'),data.get('reason',''))
                            previous=instance or data.get('instance_id');break
            except TimeoutError:
                raise RuntimeError('Response wait timed out. Work remains inspectable; signal='+key+' instance='+str(instance)+'. Use tools/soul.py inspect or episode.attach before retrying.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ws',default='ws://127.0.0.1:8765');p.add_argument('--root',default='last-cat')
    p.add_argument('--purpose',default='Explore stories, music and understanding together');p.add_argument('--user',default='user');p.add_argument('--name',default='User');p.add_argument('--soul')
    p.add_argument('--tokens',type=int,default=1024);p.add_argument('--timeout',type=float,default=300)
    try:asyncio.run(chat(p.parse_args()))
    except KeyboardInterrupt:print('\nChat disconnected. The runtime and its bounded episode may still be running.')
    except Exception as error:print(str(error),file=sys.stderr);sys.exit(1)
