"""File/WebSocket orchestration only. All sequence training executes in C++."""
import json,os,pathlib,hashlib,asyncio
from curriculum_runner import ROOT,atomic,load,publish
from ws_client import Client
from process_errors import CommandFailure, exit_description

async def run(config,curriculum='curricula/english-v2.json',epochs=8,output=None,root=None):
    from workshop import command
    data=load(ROOT/curriculum)
    if root:
        from english_review import review
        source=await export(config,root,str(pathlib.Path(config['runs'])/'english-reviewed'/f'{hashlib.sha256(root.encode()).hexdigest()[:16]}.json'))
        ids=[r['source_instance'] for r in source['lessons'] if r.get('review_status')=='operator_approved']
        if ids:data=review(source,data,ids)
    identity=hashlib.sha256(json.dumps(data,sort_keys=True).encode()+str(epochs).encode()).hexdigest()[:16]
    directory=ROOT/config['build_dir'];trainer=directory/('wds-train.exe' if os.name=='nt' else 'wds-train')
    if not trainer.exists():trainer=directory/'Release'/trainer.name
    if not trainer.exists():raise ValueError('Build/start the current runtime before training.')
    candidate=(ROOT/output if output else ROOT/config['package_root']/'candidates'/('english-'+identity)).resolve()
    run_dir=ROOT/config['runs']/('english-'+identity);run_dir.mkdir(parents=True,exist_ok=True)
    atomic(run_dir/'curriculum.json',data)
    job={'curriculum':str((run_dir/'curriculum.json').resolve()),'parent':str((ROOT/config['initial_package']).resolve()),
         'output':str(candidate),'epochs':epochs,'learning_rate':.08}
    atomic(run_dir/'job.json',job)
    publish('english.phase',stage='training or resuming',run_directory=str(run_dir))
    await command(trainer,'--language-job',run_dir/'job.json',log_path=run_dir/'trainer.log')
    atomic(run_dir/'evaluate.json',{'package':str(candidate),'curriculum':str((run_dir/'curriculum.json').resolve())})
    publish('english.phase',stage='evaluating candidate',package=str(candidate))
    process=await asyncio.create_subprocess_exec(str(trainer),'--language-evaluate',str(run_dir/'evaluate.json'),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
    try:
        stdout,stderr=await process.communicate()
        log_path=run_dir/'evaluation.log'
        with log_path.open('ab') as log:log.write(b'\nSTDOUT\n'+stdout+b'\nSTDERR\n'+stderr+('\nPROCESS '+exit_description(process.returncode)+'\n').encode('utf-8'))
        if process.returncode:raise CommandFailure(str(trainer),process.returncode,stderr.decode(errors='replace') or stdout.decode(errors='replace'),log_path)
    finally:
        if process.returncode is None:process.terminate();await process.wait()
    report=json.loads(stdout);report.update(package=str(candidate),activated=False)
    atomic(run_dir/'report.json',report)
    publish('english.phase',stage='registering candidate',package=str(candidate))
    async with Client(f"ws://127.0.0.1:{config['port']}") as client:
        manifest=load(candidate/'manifest.json')
        available=await client.call('package.register','package.available',path=candidate.relative_to((ROOT/config['package_root']).resolve()).as_posix(),checksum=manifest['checksum_fnv1a64'])
        report['package_key']=available['data']['key']
    atomic(run_dir/'report.json',report)
    publish('english.candidate',package=str(candidate),report=str(run_dir/'report.json'),activated=False)
    return report

async def export(config,root,output):
    async with Client(f"ws://127.0.0.1:{config['port']}") as client:
        lessons=[];after=''
        while True:
            result=await client.call('language.lessons','language.lessons',root_id=root,after=after)
            lessons.extend(result['data']['lessons']);after=result['data']['next_cursor']
            if not after:break
        result['data']['lessons']=lessons
    path=ROOT/output;path.parent.mkdir(parents=True,exist_ok=True);atomic(path,result['data'])
    publish('english.lessons_exported',path=str(path),message='Teacher demonstrations retain their explicit review status.')
    return result['data']
