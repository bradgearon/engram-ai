"""Real event-driven scheduler and web-worker protocol, with a deterministic HTTP fixture."""
import asyncio
import json
import pathlib
import socket
import sys
import tempfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from ws_client import Client
from mesh_web_worker import run


async def test(binary, package):
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0)); port = s.getsockname()[1]
    endpoint = f'ws://127.0.0.1:{port}'
    with tempfile.TemporaryDirectory() as tmp:
        proc = await asyncio.create_subprocess_exec(binary, '--language-mode', 'legacy', '--data', tmp, '--package', package, '--port', str(port),
                                                   stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        worker = None
        try:
            assert b'READY' in await asyncio.wait_for(proc.stdout.readline(), 30)
            def search(query):
                assert query == 'orbital resonance'
                return [{'url': 'https://en.wikipedia.org/wiki/Orbital_resonance', 'title': 'Orbital resonance',
                         'text': 'Orbital resonance is a periodic gravitational relationship.'}]
            worker = asyncio.create_task(run(endpoint, search))
            async with Client(endpoint) as client:
                await client.call('upkeep.watch', 'upkeep.snapshot')
                await client.call('root.select', 'root.selected', root_id='space', purpose='Astronomy and orbital dynamics')
                await client.call('mesh.configure', 'mesh.configured', expected_version=0,
                                  settings={'web_enabled': True, 'refractory_ms': 10})
                result = await client.call('mesh.signal', 'mesh.signal_queued', root_id='space', node_id='space', text='orbital resonance')
                key = result['data']['pulse_key']
                while True:
                    e = await client.receive({'mesh.impulse', 'token'}, timeout=15)
                    assert e['type'] != 'token', 'mesh unexpectedly generated text'
                    if e['data']['pulse_key'] == key and e['data']['status'] == 'completed': break
                assert e['data']['decision']['action'] == 'recall'
                sources = e['data']['decision']['evidence']
                assert sources and sources[0]['claim_status'] == 'hypothetical'
                snapshot = (await client.call('mesh.inspect', 'mesh.snapshot', root_id='space'))['data']
                assert snapshot['pending'] == 0 and snapshot['running'] == 0
                assert snapshot['usage']['web_queries'] == 1
                # The user's newly recorded question must not answer itself.
                await client.call('project.create', 'project.saved', project_id='chatspace', name='Astronomy', purpose='Astronomy')
                await client.call('chat.create', 'chat.saved', chat_id='webchat', project_id='chatspace', title='Knowledge lookup')
                await client.call('chat.send', 'chat.message', chat_id='webchat', text='orbital resonance')
                while True:
                    message = await client.receive({'chat.message'}, timeout=15)
                    if message['data'].get('workspace', {}).get('message', {}).get('role') == 'assistant': break
                history = (await client.call('chat.history', 'chat.history', chat_id='webchat'))['data']
                assert 'https://en.wikipedia.org/wiki/' in history['messages'][-1]['text']
                assert history['chat']['pending_signal'] == ''
                assert (await client.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']['usage']['web_queries'] == 2
                # Cancelling a queued chat releases its pending state without executing it.
                settings = (await client.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']['settings']
                await client.call('mesh.configure', 'mesh.configured', expected_version=settings['version'], settings={'enabled': False})
                await client.call('chat.send', 'chat.message', chat_id='webchat', text='Wait for this later')
                history = (await client.call('chat.history', 'chat.history', chat_id='webchat'))['data']
                await client.call('mesh.pulse.cancel', 'mesh.pulse_changed', root_id='chatspace', pulse_key=history['chat']['pending_signal'])
                history = (await client.call('chat.history', 'chat.history', chat_id='webchat'))['data']
                assert history['chat']['pending_signal'] == ''
                assert history['messages'][-1]['status'] == 'cancelled'
                await client.send({'type': 'runtime.shutdown', 'request_id': 'shutdown'})
            await asyncio.wait_for(proc.wait(), 10)
            assert proc.returncode == 0, (await proc.stderr.read()).decode()
            events = [json.loads(line) for line in pathlib.Path(tmp, 'events.jsonl').read_text().splitlines()]
            assert sum(e['type'] == 'mesh.web_received' for e in events) == 2
            assert not any(e['type'] == 'wind.activated' for e in events)
        finally:
            if worker: worker.cancel(); await asyncio.gather(worker, return_exceptions=True)
            if proc.returncode is None: proc.terminate(); await proc.wait()
    print('WebSocket mesh traversal, one-shot timer, web worker, provenance and no generation/activation passed.')


if __name__ == '__main__':
    asyncio.run(test(*sys.argv[1:]))
