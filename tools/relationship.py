#!/usr/bin/env python3
"""Explicit, idempotent relationship declarations over WebSocket; no polling."""
import argparse
import asyncio
import json
import uuid
import websockets

async def run(args):
    request = args.request_id or uuid.uuid4().hex
    command = {'schema': 1, 'type': 'heart.inspect' if args.inspect else 'relationship.declare',
        'request_id': request, 'root_id': args.root}
    if not args.inspect:
        if not all((args.soul, args.name, args.purpose)):
            raise ValueError('--soul, --name and --purpose are required for a declaration')
        command.update(soul_id=args.soul, soul_name=args.name, purpose=args.purpose,
            relationship_to=args.to or args.root, relationship=args.relationship,
            voice_id=args.voice, expected_version=args.expected_version, paused=args.paused)
    async with websockets.connect(args.ws, ping_interval=None, max_size=262144, origin=None) as ws:
        await ws.send(json.dumps(command))
        async for raw in ws:
            event = json.loads(raw)
            if event.get('type') == 'hello':
                continue
            if event.get('trace_id', event.get('request_id')) != request:
                continue
            if event['type'] == 'error':
                raise RuntimeError(event['message'])
            if event['type'] in ('relationship.declared', 'heart.snapshot'):
                print(json.dumps(event, indent=2))
                return
        raise RuntimeError('connection closed before acknowledgment')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ws', default='ws://127.0.0.1:8765')
    parser.add_argument('--root', required=True)
    parser.add_argument('--inspect', action='store_true')
    parser.add_argument('--request-id', help='Reuse this ID and the identical declaration for a safe retry')
    parser.add_argument('--soul')
    parser.add_argument('--name')
    parser.add_argument('--purpose')
    parser.add_argument('--to', help='Existing soul in the same heart; defaults to the root')
    parser.add_argument('--relationship', default='creative collaborator')
    parser.add_argument('--voice', default='purpose')
    parser.add_argument('--expected-version', type=int, default=0)
    parser.add_argument('--paused', action='store_true')
    asyncio.run(run(parser.parse_args()))
