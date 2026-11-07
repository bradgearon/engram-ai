"""Thin WebSocket/file orchestration for the native Language Wind trainer."""
import asyncio, hashlib, json, os, pathlib
from curriculum_runner import ROOT, atomic, load, publish
from ws_client import Client

async def collect(client, root, examples):
    await client.call('upkeep.watch', 'upkeep.snapshot')
    await client.call('root.select', 'root.selected', root_id=root, purpose='Language Wind examples and explicit learning')
    snapshot = (await client.call('cognition.inspect', 'cognition.snapshot', root_id=root))['data']
    existing = {r['text']: r for r in snapshot['language_samples']}
    selected = []
    for index, example in enumerate(examples):
        if example['text'] in existing:
            row = existing[example['text']]
            if row['split'] != example['split'] or row['target'] != ['greeting','help','inspect'].index(example['label']):
                raise ValueError('Existing language example has a different split/label; use a new training root.')
        else:
            request = 'language-' + hashlib.sha256((root+json.dumps(example,sort_keys=True)).encode()).hexdigest()[:32]
            acknowledged=await client.call('mesh.signal','mesh.signal_queued',request=request,root_id=root,node_id=root,
                text='/cognition '+json.dumps({'op':'language.sample', **example}))
            pulse=acknowledged['data']['pulse_key']
            # One transition-time snapshot closes the retry/acknowledgment race.
            current=(await client.call('cognition.inspect','cognition.snapshot',root_id=root))['data']
            row=next((r for r in current['language_samples'] if r['text']==example['text']),None)
            if row is None:
                publish('language.waiting',pulse_key=pulse,completed=index,total=len(examples))
                while True:
                    event=await client.receive({'cognition.committed','mesh.impulse'},pulse)
                    if event['type']=='mesh.impulse' and event['data']['status'] in ('failed','held','cancelled'):
                        raise RuntimeError(event['data'].get('reason',event['data']['status']))
                    if event['type']=='cognition.committed':
                        row=event['data']['trace']['claim'];break
        selected.append(row)
        publish('language.encoded', completed=index+1, total=len(examples), generated_tokens=0)
    return selected

async def run(config, root='language-workshop', curriculum='curricula/language-v1.json'):
    examples = load(ROOT / curriculum)['examples']
    if not 6 <= len(examples) <= 128:
        raise ValueError('Language curriculum needs 6..128 examples.')
    if len({e['text'].strip().casefold() for e in examples}) != len(examples):
        raise ValueError('Duplicate language texts across splits.')
    for e in examples:
        if set(e) != {'text','label','split'} or e['label'] not in ('greeting','help','inspect') or e['split'] not in ('train','validation','test'):
            raise ValueError('Each example needs text, label and train/validation/test split.')
    for split in ('train','validation','test'):
        if {e['label'] for e in examples if e['split']==split} != {'greeting','help','inspect'}:
            raise ValueError('Every split must cover all three language intents.')
    directory = ROOT / config['build_dir']
    trainer = directory / ('wds-train.exe' if os.name=='nt' else 'wds-train')
    if not trainer.exists(): trainer=directory/'Release'/trainer.name
    if not trainer.exists(): raise ValueError('Build the current runtime first with workshop.py start.')
    async with Client(f"ws://127.0.0.1:{config['port']}") as client:
        rows = await collect(client, root, examples)
        identities = {r['representation_id'] for r in rows}
        if len(identities)!=1: raise ValueError('Model changed between examples. Use a new root and encode again.')
        identity = identities.pop()
        fingerprint=hashlib.sha256(json.dumps(rows,sort_keys=True).encode()).hexdigest()[:16]
        output=(ROOT/config['package_root']/'candidates'/('language-'+fingerprint)).resolve()
        run_dir=ROOT/config['runs']/('language-'+fingerprint);run_dir.mkdir(parents=True,exist_ok=True)
        job={'domain':'language','feature_schema':'language.projection/1','representation_id':identity,
             'parent':str((ROOT/config['initial_package']).resolve()),'output':str(output),
             'epochs':160,'learning_rate':0.05,'backend':'scalar-sgd' if config.get('trainer_backend','auto')=='auto' else config['trainer_backend'],
             'train':[r for r in rows if r['split']=='train'],'validation':[r for r in rows if r['split']=='validation']}
        atomic(run_dir/'job.json',job)
        from workshop import command
        await command(trainer,'--job',run_dir/'job.json')
        # Locked test is read only, after training. Never feeds the optimizer.
        test={'package':str(output),'rows':[r for r in rows if r['split']=='test'],
              'minimum_strength':.75,'minimum_margin':.25}
        atomic(run_dir/'test.json',test)
        proc=await asyncio.create_subprocess_exec(str(trainer),'--evaluate',str(run_dir/'test.json'),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        try:
            stdout,stderr=await asyncio.wait_for(proc.communicate(),120)
        except BaseException:
            if proc.returncode is None:proc.kill();await proc.wait()
            raise
        if proc.returncode:raise RuntimeError(stderr.decode(errors='replace'))
        report=json.loads(stdout);report['representation_id']=identity;report['package']=str(output)
        atomic(run_dir/'report.json',report)
        manifest=load(output/'manifest.json')
        registered=await client.call('package.register','package.available',path=output.relative_to((ROOT/config['package_root']).resolve()).as_posix(),checksum=manifest['checksum_fnv1a64'])
        report['package_key']=registered['data']['key'];report['activated']=False
        publish('language.candidate', **report)
        return report
