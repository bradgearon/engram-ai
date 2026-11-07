#!/usr/bin/env python3
"""Teach understanding, ask for assembled replies, inspect provenance and train expression Winds."""
import argparse
import asyncio
import json
import pathlib
from ws_client import Client


async def main(args):
    if args.command == 'train':
        from workshop import configuration, build
        from neural_workshop import train
        config = configuration(args.config)
        _, trainer = await build(config)
        candidate, report = await train(config, args.endpoint, trainer, args.root, 'expression')
        print(json.dumps({'candidate': str(candidate), 'report': report, 'activated': False}, indent=2))
        return
    async with Client(args.endpoint) as client:
        if args.command in ('ask', 'teach', 'demo'):
            workspace = (await client.call('workspace.snapshot', 'workspace.snapshot'))['data']
            if not any(p['id'] == args.root for p in workspace['projects']):
                await client.call('project.create', 'project.saved', project_id=args.root, name=args.root,
                                  purpose='Conversation and explicitly taught understanding')
        if args.command in ('teach', 'demo'):
            values = dict(engram_id='cat-definition', subject='The Last Cat', predicate='is',
                          object='an exploration game about missing cats, memory and returning light',
                          source='Explicit demonstration lesson', expected_version=0, exclusive=True) if args.command == 'demo' else {
                'engram_id': args.id, 'subject': args.subject, 'predicate': args.predicate, 'object': args.object,
                'source': args.source, 'expected_version': args.version, 'exclusive': args.exclusive}
            result = await client.call('assembly.teach', 'assembly.taught', root_id=args.root,
                                      request='engram-demo-lesson-1' if args.command == 'demo' else None, **values)
            print(json.dumps(result['data'], indent=2))
            if args.command == 'teach': return
        if args.command in ('ask', 'demo'):
            chat = args.chat or ('engram-chat-' + args.root)
            if not any(c['id'] == chat for c in workspace['chats']):
                await client.call('chat.create', 'chat.saved', chat_id=chat, project_id=args.root, title='Engram conversation')
            else:
                existing = next(c for c in workspace['chats'] if c['id'] == chat)
                if existing['project_id'] != args.root or existing['mode'] != 'wind':
                    raise ValueError('Choose a neuronal chat belonging to this root')
            await client.call('upkeep.watch', 'upkeep.snapshot')
            extra = {'wind_instance_id': args.wind_instance} if getattr(args, 'wind_instance', None) else {}
            await client.call('chat.send', 'chat.message', chat_id=chat,
                              text='What is The Last Cat?' if args.command == 'demo' else args.text, **extra)
            while True:
                event = await client.receive({'chat.message', 'assembly.updated'}, timeout=330)
                message = event['data'].get('workspace', {}).get('message', {})
                if message.get('chat_id') == chat and message.get('role') == 'assistant':
                    print(message['text'])
                    print(json.dumps({'assembly_key': message.get('assembly_key'), 'status': message['status'],
                                      'speaker': message.get('speaker'), 'expression_package': message.get('expression_package')}, indent=2))
                    return
        elif args.command == 'inspect':
            result = await client.call('assembly.inspect', 'assembly.snapshot', root_id=args.root, assembly_key=args.key)
        elif args.command == 'list':
            result = await client.call('assembly.list', 'assembly.listed', root_id=args.root)
        elif args.command == 'feedback':
            result = await client.call('assembly.feedback', 'assembly.feedback_recorded', root_id=args.root,
                                      assembly_key=args.key, action=args.action, note=args.note)
        elif args.command == 'attune':
            result = await client.call('assembly.attune', 'assembly.attuned', root_id=args.root,
                                      engram_id=args.id, expected_version=args.version, importance=args.importance)
        elif args.command == 'export':
            result = await client.call('assembly.training.export', 'assembly.training.rows', root_id=args.root)
            pathlib.Path(args.output).write_text(json.dumps(result['data'], indent=2) + '\n', encoding='utf-8')
        elif args.command == 'activate':
            active = (await client.call('package.list', 'package.snapshot'))['data']['active']['expression']
            result = await client.call('wind.activate', 'wind.activated', domain='expression', package_key=args.key, expected_active_key=active)
        print(json.dumps(result['data'], indent=2))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--endpoint', default='ws://127.0.0.1:8765'); p.add_argument('--root', default='home')
    sub = p.add_subparsers(dest='command', required=True)
    s = sub.add_parser('ask'); s.add_argument('text'); s.add_argument('--chat'); s.add_argument('--wind-instance')
    s = sub.add_parser('demo'); s.add_argument('--chat')
    s = sub.add_parser('teach'); s.add_argument('--id', required=True); s.add_argument('--subject', required=True)
    s.add_argument('--predicate', default='is'); s.add_argument('--object', required=True); s.add_argument('--source', required=True)
    s.add_argument('--version', type=int, default=0); s.add_argument('--exclusive', action='store_true')
    sub.add_parser('list')
    s = sub.add_parser('inspect'); s.add_argument('key')
    s = sub.add_parser('feedback'); s.add_argument('key'); s.add_argument('action'); s.add_argument('--note', default='')
    s = sub.add_parser('attune'); s.add_argument('--id', required=True); s.add_argument('--version', type=int, required=True); s.add_argument('--importance', type=float, required=True)
    s = sub.add_parser('export'); s.add_argument('output')
    s = sub.add_parser('train'); s.add_argument('--config', default='configs/offline.json')
    s = sub.add_parser('activate'); s.add_argument('key')
    try: asyncio.run(main(p.parse_args()))
    except KeyboardInterrupt: pass
