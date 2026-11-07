#!/usr/bin/env python3
"""Operate the native neuronal mesh over WebSockets; no polling or ML framework."""
import argparse
import asyncio
import json
import pathlib
import uuid
from ws_client import Client


async def main(args):
    if args.command == 'train':
        from workshop import configuration, build
        from neural_workshop import train
        config = configuration(args.config)
        _, trainer = await build(config)
        candidate, report = await train(config, args.endpoint, trainer, args.root)
        print(json.dumps({'candidate': str(candidate), 'report': report, 'activated': False}, indent=2))
        return
    async with Client(args.endpoint) as client:
        if args.command == 'inspect':
            result = await client.call('mesh.inspect', 'mesh.snapshot', root_id=args.root)
        elif args.command == 'watch':
            await client.call('upkeep.watch', 'upkeep.snapshot', root_id=args.root)
            while True:
                e = json.loads(await client.ws.recv())
                if e.get('type', '').startswith('mesh.'):
                    print(json.dumps(e), flush=True)
        elif args.command == 'configure':
            current = (await client.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']
            result = await client.call('mesh.configure', 'mesh.configured',
                                      expected_version=current['settings']['version'], settings=json.loads(args.settings))
        elif args.command == 'feedback':
            result = await client.call('mesh.feedback', 'mesh.feedback_recorded', root_id=args.root,
                                      pulse_key=args.pulse, action=args.action, note=args.note)
        elif args.command == 'activate':
            current = (await client.call('package.list', 'package.snapshot'))['data']
            result = await client.call('wind.activate', 'wind.activated', domain='neural',
                                      package_key=args.package_key, expected_active_key=current['active']['neural'])
        elif args.command in ('demo', 'signal'):
            await client.call('upkeep.watch', 'upkeep.snapshot', root_id='*')
            if args.command == 'demo':
                await client.call('root.select', 'root.selected', root_id=args.root, purpose='Music motif and feedback practice')
                snapshot = (await client.call('mesh.inspect', 'mesh.snapshot', root_id=args.root))['data']
                nodes = {n['id']: n for n in snapshot['nodes']}
                for node_id, purpose in [('motif', 'Music motif and melodic movement'), ('reflection', 'Music feedback and learning')]:
                    if node_id not in nodes:
                        await client.call('mesh.node.register', 'mesh.node_registered', root_id=args.root,
                                          node_id=node_id, purpose=purpose, expected_version=0, web=False)
                if not any(r['from'] == args.root + '/motif' and r['to'] == args.root + '/reflection' for r in snapshot['routes']):
                    await client.call('mesh.route.connect', 'mesh.route_connected', root_id=args.root,
                                      **{'from': 'motif', 'to': 'reflection', 'expected_version': 0})
                packet = 'lesson-' + uuid.uuid4().hex
                import time
                await client.call('memory.upsert', 'memory.accepted', packet={
                    'schema': 1, 'intent': 'memory.upsert', 'root_id': args.root, 'engram_id': packet,
                    'packet_id': packet, 'expected_version': 0, 'hops': 0, 'expires_ms': int(time.time() * 1000) + 60000,
                    'text': 'A music motif can return to its home note; leave room for a pause.',
                    'source': 'Demo lesson, explicitly supplied by the operator', 'claim_status': 'observed'})
                node, text = 'motif', 'Recall the music motif lesson'
            else:
                node, text = args.node or args.root, args.text
            accepted = await client.call('mesh.signal', 'mesh.signal_queued', root_id=args.root, node_id=node, text=text)
            key = accepted['data']['pulse_key']
            print(json.dumps(accepted), flush=True)
            while True:
                result = await client.receive({'mesh.impulse', 'mesh.deferred', 'mesh.web_expired', 'mesh.web_received', 'mesh.pulse_changed'}, timeout=180)
                if result['data'].get('pulse_key') == key:
                    print(json.dumps(result, indent=2), flush=True)
                    if result['data']['status'] not in ('queued', 'running', 'waiting_web'):
                        return
                elif any(p['key'] == key and p['status'] not in ('queued', 'running', 'waiting_web')
                         for p in result['data'].get('mesh', {}).get('pulses', [])):
                    print(json.dumps(result, indent=2), flush=True)
                    return
        elif args.command == 'export':
            result = await client.call('mesh.training.export', 'mesh.training.rows', root_id=args.root)
            pathlib.Path(args.output).write_text(json.dumps(result['data'], indent=2) + '\n', encoding='utf-8')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--endpoint', default='ws://127.0.0.1:8765')
    p.add_argument('--root', default='mesh-demo')
    sub = p.add_subparsers(dest='command', required=True)
    sub.add_parser('inspect'); sub.add_parser('watch'); sub.add_parser('demo')
    s = sub.add_parser('signal'); s.add_argument('text'); s.add_argument('--node')
    s = sub.add_parser('configure'); s.add_argument('settings', help='JSON settings delta')
    s = sub.add_parser('feedback'); s.add_argument('pulse'); s.add_argument('action'); s.add_argument('--note', default='')
    s = sub.add_parser('export'); s.add_argument('output')
    s = sub.add_parser('train'); s.add_argument('--config', default='configs/offline.json')
    s = sub.add_parser('activate'); s.add_argument('package_key')
    try:
        asyncio.run(main(p.parse_args()))
    except KeyboardInterrupt:
        pass
