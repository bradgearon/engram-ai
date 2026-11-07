"""Native expression training, CLI conversation, Arc recovery and explicit activation; no browser."""
import asyncio
import json
import pathlib
import socket
import subprocess
import sys
import tempfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from neural_workshop import split_samples
from ws_client import Client


async def test(trainer, server, parent):
    tools = pathlib.Path(__file__).resolve().parents[1] / 'tools'
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        samples = []
        # Synthetic reply-plan labels only. This does not test natural-language understanding.
        for target, lane in [(0, 0), (2, 5), (3, 2)]:
            for i in range(30):
                x = [0.] * 16; x[lane] = .7 + i / 100; x[15] = 1
                samples.append({'features': x, 'target': target, 'lineage': f'plan-{target}-{i}'})
        splits = split_samples(samples)
        candidate = root / 'candidate'
        job = {'domain': 'expression', 'parent': parent, 'output': str(candidate), 'epochs': 100,
               'learning_rate': .05, 'backend': 'scalar-sgd', 'train': splits['train'], 'validation': splits['validation']}
        (root / 'job.json').write_text(json.dumps(job))
        trained = subprocess.run([trainer, '--job', str(root / 'job.json')], capture_output=True, text=True, timeout=60)
        assert trained.returncode == 0, trained.stderr
        manifest = json.loads((candidate / 'manifest.json').read_text())
        assert manifest['feature_schema'] == 'expression.features/1'
        assert (candidate / 'policy.wind').stat().st_size == 1152
        (root / 'test.json').write_text(json.dumps({'package': str(candidate), 'rows': splits['test']}))
        report = json.loads(subprocess.check_output([trainer, '--evaluate', str(root / 'test.json')]))
        assert report['accuracy'] > .95 and report['activated'] is False, report
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0)); port = s.getsockname()[1]
        endpoint = f'ws://127.0.0.1:{port}'
        data = root / 'arc'

        async def start():
            p = await asyncio.create_subprocess_exec(server, '--language-mode', 'legacy', '--data', str(data), '--package', parent,
                '--packages', str(root), '--port', str(port), stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
            assert b'READY' in await asyncio.wait_for(p.stdout.readline(), 30)
            return p

        async def cli(*args):
            p = await asyncio.create_subprocess_exec(sys.executable, str(tools / 'engram.py'),
                '--endpoint', endpoint, '--root', 'unit', *args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
            out, err = await asyncio.wait_for(p.communicate(), 30)
            assert p.returncode == 0, err.decode()
            return out.decode()

        proc = await start()
        try:
            assert 'returning light' in await cli('demo')
            async with Client(endpoint) as client:
                await client.call('upkeep.watch', 'upkeep.snapshot')
                settings = (await client.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']['settings']
                await client.call('mesh.configure', 'mesh.configured', expected_version=settings['version'],
                    settings={'refractory_ms': 10, 'max_knowledge_calls': 0, 'max_impulses_per_window': 1024, 'enabled': False})
                active = (await client.call('package.list', 'package.snapshot'))['data']['active']['expression']
                registered = await client.call('package.register', 'package.available', path='candidate', checksum=manifest['checksum_fnv1a64'])
                key = registered['data']['key']
                assert (await client.call('package.list', 'package.snapshot'))['data']['active']['expression'] == active
                queued = await client.call('mesh.signal', 'mesh.signal_queued', root_id='unit', node_id='unit', text='hello', response=True)
                try:
                    await client.call('wind.activate', 'wind.activated', domain='expression', package_key=key, expected_active_key=active)
                    raise AssertionError('activation during collecting response was accepted')
                except RuntimeError as error:
                    assert 'boundary' in str(error) or 'assembl' in str(error), error
                await client.call('mesh.pulse.cancel', 'mesh.pulse_changed', root_id='unit', pulse_key=queued['data']['pulse_key'])
                await client.call('wind.activate', 'wind.activated', domain='expression', package_key=key, expected_active_key=active)
                settings = (await client.call('mesh.inspect', 'mesh.snapshot', summary=True))['data']['settings']
                await client.call('mesh.configure', 'mesh.configured', expected_version=settings['version'], settings={'enabled': True})
                task = {'schema': 1, 'id': 'wind-creation', 'domain': 'music',
                        'artifact': {'notes': [{'pitch': 60, 'duration': 2}, {'pitch': 64, 'duration': 2},
                                               {'pitch': 67, 'duration': 2}, {'pitch': 60, 'duration': 2}]},
                        'requirements': {'pitch_min': 48, 'pitch_max': 84, 'target_ticks': 8, 'home_pc': 0, 'return_home': True}}
                episode = await client.episode('expression-wind-source', root_id='unit', task=task, max_steps=1, mode='fixed_refine')
                assert episode['status'] == 'completed', episode
            wind_output = await cli('ask', 'Review the Wind music creation', '--wind-instance', episode['instance_id'])
            assert 'Wind episode' in wind_output and 'pitch' in wind_output, wind_output
            await cli('teach', '--id', 'cat-definition', '--version', '1', '--subject', 'The Last Cat',
                      '--object', 'a game about rescuing stars', '--source', 'Corrected project notes', '--exclusive')
            await cli('attune', '--id', 'cat-definition', '--version', '2', '--importance', '0.8')
            output = await cli('ask', 'What is The Last Cat?')
            assert 'rescuing stars' in output and 'version 3' in output, output
            async with Client(endpoint) as client:
                listed = (await client.call('assembly.list', 'assembly.listed', root_id='unit'))['data']['assemblies']
                assembly = (await client.call('assembly.inspect', 'assembly.snapshot', root_id='unit', assembly_key=listed[0]['key']))['data']
                assert assembly['expression_package'] == key
                assert assembly['expression']['method'] == 'policy.wind/expression.features/1'
                assert assembly['expression']['generated_tokens'] == 0
                assert assembly['weight_bindings'][0]['checksum'] == manifest['checksum_fnv1a64']
                assert assembly['weight_bindings'][0]['rows'] == list(range(7))
                await client.call('assembly.feedback', 'assembly.feedback_recorded', root_id='unit', assembly_key=assembly['key'], action='answer')
                rows = (await client.call('assembly.training.export', 'assembly.training.rows', root_id='unit'))['data']['samples']
                assert len(rows) == 1 and len(rows[0]['features']) == 16
                await client.send({'type': 'runtime.shutdown', 'request_id': 'stop'})
            await asyncio.wait_for(proc.wait(), 10)
            assert proc.returncode == 0, (await proc.stderr.read()).decode()
            # Simulate a crash after durable assembly completion but before chat delivery.
            journal = data / 'events.jsonl'
            lines = journal.read_text().splitlines()
            completion = next(i for i, line in enumerate(lines) if json.loads(line)['type'] == 'assembly.completed'
                              and json.loads(line)['data']['assembly_key'] == assembly['key'])
            journal.write_text('\n'.join(lines[:completion + 1]) + '\n')
            for _ in range(2):
                proc = await start()
                async with Client(endpoint) as client:
                    history = (await client.call('chat.history', 'chat.history', chat_id='engram-chat-unit'))['data']
                    delivered = [m for m in history['messages'] if m.get('assembly_key') == assembly['key']]
                    assert len(delivered) == 1 and 'rescuing stars' in delivered[0]['text'], delivered
                    assert history['chat']['pending_signal'] == ''
                    await client.send({'type': 'runtime.shutdown', 'request_id': f'restart-stop-{_}'})
                await asyncio.wait_for(proc.wait(), 10)
                assert proc.returncode == 0, (await proc.stderr.read()).decode()
        finally:
            if proc.returncode is None: proc.terminate(); await proc.wait()
    print('Native expression candidate ABI/gate, explicit boundary activation, CLI teaching/correction, Arc weights references and exactly-once restart delivery passed.')


if __name__ == '__main__':
    asyncio.run(test(*sys.argv[1:]))
