"""Prepare split manifests and supervise the native readout optimizer; no Python ML."""
import asyncio
import hashlib
import json
import pathlib
from curriculum_runner import ROOT, atomic, load, publish
from ws_client import Client


async def native(binary, flag, path=None):
    process = await asyncio.create_subprocess_exec(str(binary), flag, *([str(path)] if path is not None else []),
                                                 stdout=asyncio.subprocess.PIPE,
                                                 stderr=asyncio.subprocess.PIPE)
    errors = asyncio.create_task(process.stderr.read())
    result = None
    try:
        async for line in process.stdout:
            result = json.loads(line)
            publish('neural.native', event=result)
        if await process.wait():
            raise RuntimeError((await errors).decode(errors='replace'))
        return result
    finally:
        if process.returncode is None:
            process.terminate()
            await process.wait()
        await errors


def split_samples(samples):
    # Connected impulses and identical observations must not cross split boundaries.
    groups = {}
    lineage_groups = {}
    for sample in samples:
        signature = hashlib.sha256(json.dumps(sample['features'], separators=(',', ':')).encode()).hexdigest()
        lineage = sample['lineage']
        keys = {signature, *lineage_groups.get(lineage, set())}
        for group in list(groups):
            if keys.intersection(groups[group]['keys']):
                keys.update(groups[group]['keys'])
        combined = [sample]
        for group in list(groups):
            if keys.intersection(groups[group]['keys']):
                combined.extend(groups.pop(group)['samples'])
        owner = min(keys)
        groups[owner] = {'keys': keys, 'samples': combined}
        lineage_groups[lineage] = keys
    # Final union covers transitive overlaps caused by a later shared lineage.
    changed = True
    while changed:
        changed = False
        keys = list(groups)
        for i, a in enumerate(keys):
            if a not in groups: continue
            for b in keys[i + 1:]:
                if b not in groups: continue
                la = {s['lineage'] for s in groups[a]['samples']}
                lb = {s['lineage'] for s in groups[b]['samples']}
                if groups[a]['keys'] & groups[b]['keys'] or la & lb:
                    groups[a]['keys'].update(groups[b]['keys'])
                    groups[a]['samples'].extend(groups.pop(b)['samples'])
                    changed = True
    ordered = sorted(groups.items())
    if len(ordered) < 12:
        raise ValueError('Need at least 12 independent labeled observations (different features and lineages) before splitting train/validation/locked test.')
    splits = {'train': [], 'validation': [], 'test': []}
    for i, (_, group) in enumerate(ordered):
        name = 'test' if i % 5 == 0 else 'validation' if i % 5 == 1 else 'train'
        splits[name].extend(group['samples'])
    if any(len(rows) < 2 for rows in splits.values()):
        raise ValueError('Every independent split needs at least two labeled observations')
    return splits


async def train(config, endpoint, trainer, root, domain='neural'):
    if domain not in ('neural', 'expression'):
        raise ValueError('Expected neural or expression training domain')
    namespace = 'mesh' if domain == 'neural' else 'assembly'
    async with Client(endpoint) as client:
        exported = (await client.call(namespace + '.training.export', namespace + '.training.rows', root_id=root))['data']
    samples = exported['samples']
    splits = split_samples(samples)
    backend = config.get('trainer_backend', 'auto')
    if backend == 'auto': backend = (await native(trainer, '--capabilities'))['default_backend']
    parent = load(ROOT / config['initial_package'] / 'manifest.json')
    identity = hashlib.sha256(json.dumps({'export': exported, 'backend': backend, 'parent': parent['checksum_fnv1a64'],
                                         'optimizer': 'readout-workshop/2'}, sort_keys=True).encode()).hexdigest()[:16]
    directory = ROOT / config['runs'] / (domain + '-' + identity)
    candidate = ROOT / config['package_root'] / 'candidates' / (domain + '-' + identity)
    directory.mkdir(parents=True, exist_ok=True)
    atomic(directory / 'splits.json', splits)
    compact = lambda values: [{'features': r['features'], 'target': r['target']} for r in values]
    job = {'schema': 1, 'domain': domain, 'parent': str(ROOT / config['initial_package']),
           'output': str(candidate), 'train': compact(splits['train']),
           'validation': compact(splits['validation']), 'epochs': 100, 'learning_rate': .05,
           'backend': backend}
    atomic(directory / 'job.json', job)
    await native(trainer, '--job', directory / 'job.json')
    # Locked test is evaluated once per immutable candidate. Its labels never enter the optimizer job.
    report_path = directory / 'report.json'
    if report_path.exists():
        report = load(report_path)
    else:
        evaluation = {'package': str(candidate), 'rows': compact(splits['test'])}
        atomic(directory / 'locked-test.json', evaluation)
        report = await native(trainer, '--evaluate', directory / 'locked-test.json')
        report.update(split_counts={name: len(rows) for name, rows in splits.items()},
                      candidate=str(candidate.relative_to(ROOT)), activation='explicit command only')
        atomic(report_path, report)
    async with Client(endpoint) as client:
        manifest = load(candidate / 'manifest.json')
        await client.call('package.register', 'package.available',
                          path=candidate.relative_to(ROOT / config['package_root']).as_posix(),
                          checksum=manifest['checksum_fnv1a64'])
    publish(domain + '.training_finished', candidate=str(candidate.relative_to(ROOT)), report=report, activated=False)
    return candidate, report
