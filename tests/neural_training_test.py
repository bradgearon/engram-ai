"""Operator feedback -> grouped splits -> native Wind -> fresh unit; activation stays explicit."""
import asyncio
import json
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from neural_workshop import split_samples
import neural_workshop
from ws_client import Client


def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0)); return s.getsockname()[1]


async def test(trainer, server, parent):
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        samples = []
        # Explicit synthetic lessons, not claims of human data or pretrained model quality.
        for label, lane in [(0, 1), (1, 2), (2, 5)]:
            for i in range(30):
                features = [0.] * 16
                features[lane] = .6 + i / 100
                features[15] = 1.
                samples.append({'features': features, 'target': label, 'lineage': f'lesson-{label}-{i}'})
        splits = split_samples(samples)
        signatures = [{json.dumps(x['features']) for x in values} for values in splits.values()]
        assert all(not signatures[i] & signatures[j] for i in range(3) for j in range(i))
        repeated = samples + [{**samples[0], 'lineage': 'another-lineage'}]
        merged = split_samples(repeated)
        assert sum(any(r['features'] == samples[0]['features'] for r in values) for values in merged.values()) == 1
        candidate = root / 'candidate'
        job = {'domain': 'neural', 'parent': parent, 'output': str(candidate), 'epochs': 100,
               'learning_rate': .05, 'backend': 'scalar-sgd', 'train': splits['train'], 'validation': splits['validation']}
        (root / 'job.json').write_text(json.dumps(job))
        trained = subprocess.run([trainer, '--job', str(root / 'job.json')], capture_output=True, text=True, timeout=60)
        assert trained.returncode == 0, trained.stderr
        manifest = json.loads((candidate / 'manifest.json').read_text())
        assert (candidate / 'policy.wind').stat().st_size == 1152
        assert manifest['feature_schema'] == 'neural.features/1'
        evaluation = {'package': str(candidate), 'rows': splits['test']}
        (root / 'test.json').write_text(json.dumps(evaluation))
        report = json.loads(subprocess.check_output([trainer, '--evaluate', str(root / 'test.json')]))
        assert report['accuracy'] >= .95 and report['coverage'] > .8, report
        assert report['activated'] is False and report['knowledge_calls'] == 0
        p = port()
        proc = await asyncio.create_subprocess_exec(server, '--language-mode', 'legacy', '--data', str(root / 'fresh-unit'), '--package', parent,
            '--packages', str(root), '--port', str(p), stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        try:
            assert b'READY' in await asyncio.wait_for(proc.stdout.readline(), 20)
            async with Client(f'ws://127.0.0.1:{p}') as client:
                await client.call('upkeep.watch', 'upkeep.snapshot')
                await client.call('root.select', 'root.selected', root_id='unit', purpose='Demonstration decisions')
                before = (await client.call('package.list', 'package.snapshot'))['data']['active']
                registered = await client.call('package.register', 'package.available', path='candidate', checksum=manifest['checksum_fnv1a64'])
                key = registered['data']['key']
                assert (await client.call('package.list', 'package.snapshot'))['data']['active'] == before
                await client.call('wind.activate', 'wind.activated', domain='neural', package_key=key, expected_active_key=before['neural'])
                row = splits['test'][0]
                accepted = await client.call('mesh.signal', 'mesh.signal_queued', root_id='unit', node_id='unit',
                                            text='Held-out structured observation', features=row['features'])
                pulse = accepted['data']['pulse_key']
                while True:
                    e = await client.receive({'mesh.impulse'}, timeout=15)
                    if e['data']['pulse_key'] == pulse: break
                decision = e['data']['decision']
                assert decision['method'] == 'policy.wind/neural.features/1'
                assert decision['action'] == ['rest', 'recall', 'propagate'][row['target']]
                assert decision['knowledge_calls'] == 0 and e['data']['status'] == 'completed'
                # Exercise the shipped export/supervisor path with journaled operator labels.
                await client.call('mesh.configure', 'mesh.configured', expected_version=0,
                                  settings={'max_impulses_per_window': 1024, 'refractory_ms': 10, 'max_knowledge_calls': 0})
                for lesson in samples:
                    accepted = await client.call('mesh.signal', 'mesh.signal_queued', root_id='unit', node_id='unit',
                                                text='Explicit synthetic teaching lesson', features=lesson['features'])
                    pulse = accepted['data']['pulse_key']
                    while True:
                        event = await client.receive({'mesh.impulse'}, timeout=15)
                        if event['data']['pulse_key'] == pulse: break
                    await client.call('mesh.feedback', 'mesh.feedback_recorded', root_id='unit', pulse_key=pulse,
                                      action=['rest', 'recall', 'propagate'][lesson['target']], note='Synthetic fixture label')
                active = (await client.call('package.list', 'package.snapshot'))['data']['active']
                neural_workshop.ROOT = root
                config = {'runs': 'runs', 'package_root': '.', 'initial_package': parent, 'trainer_backend': 'auto'}
                exported_candidate, exported_report = await neural_workshop.train(config, f'ws://127.0.0.1:{p}', pathlib.Path(trainer), 'unit')
                assert exported_candidate.is_dir() and exported_report['activated'] is False
                assert sum(exported_report['split_counts'].values()) == len(samples)
                assert (await client.call('package.list', 'package.snapshot'))['data']['active'] == active
                # Re-running the same dataset reuses the immutable candidate/report.
                again, same_report = await neural_workshop.train(config, f'ws://127.0.0.1:{p}', pathlib.Path(trainer), 'unit')
                assert again == exported_candidate and same_report == exported_report
                await client.send({'type': 'runtime.shutdown', 'request_id': 'stop'})
            await asyncio.wait_for(proc.wait(), 20)
            assert proc.returncode == 0, (await proc.stderr.read()).decode()
        finally:
            if proc.returncode is None: proc.terminate(); await proc.wait()
    print('Native neural training, disjoint groups, held-out gate, unchanged ABI, explicit activation and fresh-unit inference passed.')


if __name__ == '__main__':
    asyncio.run(test(*sys.argv[1:]))
