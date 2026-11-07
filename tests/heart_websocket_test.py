"""Acceptance: explicit declaration -> process restart -> relevant voice -> auditable result."""
import asyncio
import json
import pathlib
import socket
import sys
import tempfile
import websockets

async def receive(ws, kind, trace=None):
    while True:
        message = json.loads(await asyncio.wait_for(ws.recv(), 15))
        if message['type'] == 'error' and kind != 'error':
            raise AssertionError(message)
        if message['type'] == kind and (trace is None or message.get('trace_id') == trace):
            return message

async def send(ws, command):
    await ws.send(json.dumps({'schema': 1, **command}))

async def stop(process, directory):
    process.terminate()
    await asyncio.wait_for(process.wait(), 20)
    if sys.platform != 'win32':
        assert process.returncode == 0, (await process.stderr.read()).decode()
    else:
        lock = pathlib.Path(directory) / 'writer.lock'
        if lock.exists(): lock.rmdir()

async def start(directory, port):
    process = await asyncio.create_subprocess_exec(sys.argv[1], '--data', directory,
        '--package', sys.argv[2], '--port', str(port), stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    assert b'READY' in await asyncio.wait_for(process.stdout.readline(), 15)
    return process

async def main():
    with tempfile.TemporaryDirectory() as directory:
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        endpoint = f'ws://127.0.0.1:{port}'
        declaration = {'type': 'relationship.declare', 'request_id': 'declare-auerlis', 'root_id': 'composer',
            'soul_id': 'auerlis', 'soul_name': 'Auerlis', 'purpose': 'music composition',
            'relationship_to': 'composer', 'relationship': 'creative collaborator', 'voice_id': 'purpose',
            'expected_version': 0}
        process = await start(directory, port)
        try:
            async with websockets.connect(endpoint, ping_interval=None) as ws:
                await receive(ws, 'hello')
                await send(ws, {'type': 'root.select', 'request_id': 'root', 'root_id': 'composer', 'purpose': 'compose music'})
                await receive(ws, 'root.selected')
                await send(ws, declaration)
                acknowledgment = await receive(ws, 'relationship.declared')
                assert acknowledgment['data']['interest']['status'] == 'eligible'
                await send(ws, declaration)
                assert await receive(ws, 'relationship.declared') == acknowledgment
                await send(ws, {**declaration, 'purpose': 'conflicting purpose'})
                assert 'request_id conflict' in (await receive(ws, 'error'))['message']
        finally:
            await stop(process, directory)
        # New OS process: relationship and dedup ledger must come only from the journal.
        process = await start(directory, port)
        try:
            async with websockets.connect(endpoint, ping_interval=None) as ws:
                await receive(ws, 'hello')
                await send(ws, declaration)
                assert await receive(ws, 'relationship.declared') == acknowledgment
                await send(ws, {'type': 'heart.inspect', 'request_id': 'inspect', 'root_id': 'composer'})
                snapshot = (await receive(ws, 'heart.snapshot'))['data']
                assert len(snapshot['souls']) == 2 and snapshot['interests'][0]['strength'] == 1
                await send(ws, {'type': 'seed', 'mode': 'foundation_once', 'request_id': 'music', 'root_id': 'composer',
                    'text': 'Develop music composition with a recurring motif.', 'voice_budget': 2, 'max_steps': 1})
                final = (await receive(ws, 'episode.finished', 'music'))['data']
                evaluation = final['evaluation']
                assert evaluation['status'] == 'pass' and final['status'] == 'completed'
                voices = evaluation['contributions']
                assert [v['voice_id'] for v in voices] == ['constraint', 'purpose']
                assert voices[1]['represented_souls'] == ['auerlis']
                assert voices[1]['evidence'][-1]['sequence'] == acknowledgment['seq']
                assert all(v['trace_id'] == 'music' and v['root_id'] == 'composer' for v in voices)
                await send(ws, {'type': 'seed', 'mode': 'foundation_once', 'request_id': 'budget', 'root_id': 'composer',
                    'text': 'Develop music composition.', 'voice_budget': 1, 'max_steps': 1})
                limited = (await receive(ws, 'episode.finished', 'budget'))['data']
                assert limited['status'] == 'exhausted' and limited['evaluation']['status'] == 'inconclusive'
                assert limited['evaluation']['skipped'][0]['reason'] == 'budget_exhausted'
                await send(ws, {'type': 'seed', 'mode': 'foundation_once', 'request_id': 'isolated', 'root_id': 'separate',
                    'text': 'Develop music composition.', 'max_steps': 1})
                isolated = (await receive(ws, 'episode.finished', 'isolated'))['data']['evaluation']
                assert isolated['represented_souls'] == ['separate']
                assert len(isolated['contributions']) == 1
        finally:
            await stop(process, directory)
        events = [json.loads(line) for line in (pathlib.Path(directory) / 'events.jsonl').read_text().splitlines()]
        declarations = [e for e in events if e['type'] == 'relationship.declared']
        assert len(declarations) == 1
        assert all(e['seq'] == i + 1 for i, e in enumerate(events))
        assert not any(e['type'] == 'interest.accepted' for e in events)
    print('declaration/restart/consultation WebSocket acceptance passed')

asyncio.run(main())
