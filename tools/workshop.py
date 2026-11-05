#!/usr/bin/env python3
"""Launch C++ runtime/trainer, run curricula, or explicitly activate a package."""
import argparse,asyncio,hashlib,json,os,pathlib,shutil,subprocess,sys,venv
ROOT=pathlib.Path(__file__).resolve().parents[1]
# The only third-party Python runtime dependency is the WebSocket client.
try:import websockets
except ImportError:
    local=ROOT/'.workshop-venv';python=local/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
    if not python.exists():venv.EnvBuilder(with_pip=True).create(local)
    subprocess.run([str(python),'-m','pip','install','-r',str(ROOT/'tools/requirements.txt')],check=True)
    os.execv(str(python),[str(python),str(pathlib.Path(__file__).resolve()),*sys.argv[1:]])
from curriculum_runner import Runner,atomic,digest,load,publish
from ws_client import Client

async def command(*args):
    process=await asyncio.create_subprocess_exec(*map(str,args),cwd=ROOT)
    try:
        if await process.wait():raise RuntimeError('command failed: '+str(args[0]))
    finally:
        if process.returncode is None:process.terminate();await process.wait()

def configuration(path):
    config=load(ROOT/path)
    allowed={'schema','port','build_dir','ggml','llama','llama_source','model','gpu_layers','build_jobs','data','runs',
        'package_root','initial_package','teacher_ws','teacher_model','teacher_port','max_teacher_calls',
        'max_foundation_calls','run_timeout_seconds','trainer_backend','parents','upkeep','cuda'}
    if set(config)-allowed:raise ValueError('unknown configuration keys: '+','.join(sorted(set(config)-allowed)))
    if config.get('schema')!=1:raise ValueError('unknown configuration schema')
    if type(config['port']) is not int or not 1<=config['port']<=65535:raise ValueError('invalid port')
    if not 1<=config.get('build_jobs',4)<=16:raise ValueError('build_jobs must be 1..16')
    if not 1<=config.get('run_timeout_seconds',1800)<=86400:raise ValueError('invalid training time budget')
    if not 0<=config.get('max_teacher_calls',0)<=4 or not 0<=config.get('max_foundation_calls',0)<=100:raise ValueError('model call budget exceeded')
    for key in ('model','teacher_model'):
        if config.get(key) and not (ROOT/config[key]).is_file():raise ValueError(key+' GGUF path does not exist')
    return config

async def build(config):
    cmake=shutil.which('cmake')
    if not cmake:raise RuntimeError('CMake 3.20+ and a C++20 compiler are required. Install them, then rerun this command.')
    directory=(ROOT/config['build_dir']).resolve()
    llama=bool(config.get('llama') or config.get('model') or config.get('teacher_model'))
    options=['-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=OFF','-DGGML_CUDA='+('ON' if config.get('cuda',False) else 'OFF'),'-DWDS_LLAMA='+('ON' if llama else 'OFF'),
        '-DWDS_GGML_TRAIN='+('ON' if config.get('ggml',True) else 'OFF'),'-DWDS_LLAMA_SOURCE='+str((ROOT/config['llama_source']).resolve() if config.get('llama_source') else '')]
    h=hashlib.sha256(json.dumps(options).encode())
    for p in sorted([ROOT/'CMakeLists.txt',*list((ROOT/'src').glob('*.cpp')),*list((ROOT/'include').rglob('*.hpp'))]):h.update(p.read_bytes())
    stamp=directory/'workshop-build.json';same=stamp.exists() and load(stamp).get('fingerprint')==h.hexdigest()
    def binary(name):
        direct=directory/(name+('.exe' if os.name=='nt' else ''))
        return direct if direct.exists() else directory/'Release'/direct.name
    if not same or not all(binary(name).is_file() for name in ('wds-ai','wds-train')):
        publish('workshop.build',ggml=config.get('ggml',True),llama=llama)
        await command(cmake,'-S',ROOT,'-B',directory,*options)
        await command(cmake,'--build',directory,'--config','Release','-j',config.get('build_jobs',4))
        atomic(stamp,{'fingerprint':h.hexdigest()})
    return binary('wds-ai'),binary('wds-train')

class Server:
    def __init__(self,binary,config,teacher=False):self.binary=binary;self.config=config;self.teacher=teacher;self.process=None;self.drains=[]
    async def start(self):
        c=self.config;port=c.get('teacher_port',c['port']+1) if self.teacher else c['port']
        self.endpoint=f'ws://127.0.0.1:{port}'
        data=(ROOT/c['data']/('teacher' if self.teacher else 'runtime')).resolve()
        args=[str(self.binary),'--data',str(data),'--package',str((ROOT/c['initial_package']).resolve()),
            '--packages',str((ROOT/c['package_root']).resolve()),'--port',str(port)]
        model=c.get('teacher_model' if self.teacher else 'model','')
        if model:args+=['--model',str((ROOT/model).resolve()),'--gpu-layers',str(c.get('gpu_layers',0))]
        self.process=await asyncio.create_subprocess_exec(*args,cwd=ROOT,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        logdir=(ROOT/c['runs']/'logs');logdir.mkdir(parents=True,exist_ok=True)
        async def drain(stream,path):
            with path.open('ab') as file:
                while line:=await stream.readline():file.write(line);file.flush()
        self.drains.append(asyncio.create_task(drain(self.process.stderr,logdir/('teacher.log' if self.teacher else 'runtime.log'))))
        ready=await asyncio.wait_for(self.process.stdout.readline(),180)
        if not ready.startswith(b'READY'):
            raise RuntimeError('runtime failed to start; inspect '+str(logdir)+': '+ready.decode(errors='replace'))
        self.drains.append(asyncio.create_task(drain(self.process.stdout,logdir/('teacher.stdout.log' if self.teacher else 'runtime.stdout.log'))))
        async with Client(self.endpoint) as client:
            publish('workshop.ready',endpoint=self.endpoint,foundation=client.hello.get('foundation'),teacher=self.teacher)
        return self
    async def stop(self):
        if self.process and self.process.returncode is None:
            try:
                async with Client(self.endpoint) as client:await client.send({'type':'runtime.shutdown','request_id':'supervisor-stop'})
                await asyncio.wait_for(self.process.wait(),30)
            except (Exception,asyncio.CancelledError):
                if self.process.returncode is None:
                    self.process.terminate()
                    try:await asyncio.wait_for(self.process.wait(),10)
                    except asyncio.TimeoutError:self.process.kill();await self.process.wait()
        for task in self.drains:
            if not task.done():task.cancel()
        await asyncio.gather(*self.drains,return_exceptions=True)

async def start(args,config):
    runtime,trainer=await build(config);servers=[];training=None;exits=[];fetch_worker=None
    try:
        primary=Server(runtime,config);servers.append(primary);await primary.start()
        if config.get('teacher_model'):
            teacher=Server(runtime,config,True);servers.append(teacher);await teacher.start();config={**config,'teacher_ws':teacher.endpoint}
        async with Client(primary.endpoint) as client:
            snapshot=(await client.call('upkeep.inspect','upkeep.snapshot'))['data']
            settings=dict(config.get('upkeep',{}))
            if args.upkeep:settings['enabled']=True
            if settings:await client.call('upkeep.configure','upkeep.configured',expected_version=snapshot['settings']['version'],settings=settings)
        from upkeep_worker import run as material_worker
        fetch_worker=asyncio.create_task(material_worker(primary.endpoint))
        exits=[asyncio.create_task(s.process.wait()) for s in servers]
        async def train():
            for domain in args.train.split(','):
                if domain not in ('music','architecture'):raise ValueError('unknown curriculum: '+domain)
                runner=Runner(config,domain,primary.endpoint,trainer,args.resume)
                try:await asyncio.wait_for(runner.run_all(),config['run_timeout_seconds'])
                except asyncio.TimeoutError:raise RuntimeError('Training time budget expired; checkpoints remain in '+str(ROOT/config['runs'])+'; rerun start --train '+args.train+' --resume')
        if args.train:
            training=asyncio.create_task(train());done,_=await asyncio.wait([training,fetch_worker,*exits],return_when=asyncio.FIRST_COMPLETED)
            if fetch_worker in done:await fetch_worker;raise RuntimeError('material worker stopped')
            if training not in done:raise RuntimeError('runtime exited during training')
            await training
            if args.exit_after_training:return
            publish('workshop.idle',message='Training finished. Runtime remains available; activation is a separate command.')
        done,_=await asyncio.wait([*exits,fetch_worker],return_when=asyncio.FIRST_COMPLETED)
        if fetch_worker in done:await fetch_worker;raise RuntimeError('material worker stopped')
        if any(task.result()!=0 for task in done):raise RuntimeError('runtime process failed; see run logs')
    finally:
        if fetch_worker:fetch_worker.cancel();await asyncio.gather(fetch_worker,return_exceptions=True)
        if training and not training.done():training.cancel();await asyncio.gather(training,return_exceptions=True)
        for server in reversed(servers):await server.stop()
        for task in exits:
            if not task.done():task.cancel()

async def main(args):
    config=configuration(args.config);endpoint=f"ws://127.0.0.1:{config['port']}"
    if args.command=='start':return await start(args,config)
    async with Client(endpoint) as client:
        if args.command=='upkeep':
            snapshot=(await client.call('upkeep.inspect','upkeep.snapshot'))['data']
            if args.action=='status':print(json.dumps(snapshot,indent=2))
            else:print(json.dumps(await client.call('upkeep.configure','upkeep.configured',expected_version=snapshot['settings']['version'],settings={'enabled':args.action=='resume'}),indent=2))
        elif args.command in ('cancel','resume'):
            print(json.dumps(await client.call(args.command,'wind.checkpoint',instance_id=args.instance),indent=2))
        elif args.command=='status':print(json.dumps((await client.call('package.list','package.snapshot'))['data'],indent=2))
        elif args.command=='stop':await client.send({'type':'runtime.shutdown','request_id':'operator-stop'})
        elif args.command=='activate':
            path=(ROOT/args.package).resolve();manifest=load(path/'manifest.json')
            package=(await client.call('package.register','package.available',path=path.relative_to((ROOT/config['package_root']).resolve()).as_posix(),checksum=manifest['checksum_fnv1a64']))['data']['key']
            active=(await client.call('package.list','package.snapshot'))['data']['active'][args.domain]
            result=await client.call('wind.activate','wind.activated',domain=args.domain,package_key=package,expected_active_key=active)
            print(json.dumps(result,indent=2))
        elif args.command=='run':
            value=load((ROOT/args.task).resolve());task=value.get('task',value)
            await client.call('root.select','root.selected',root_id=args.root,purpose='Run a structured '+task['domain']+' task.')
            values={'root_id':args.root,'task':task,'max_steps':args.steps,'text':'Apply the active domain Wind.'}
            result=await client.episode(__import__('uuid').uuid4().hex,**values);print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--config',default='configs/local.json')
    sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('start');p.add_argument('--upkeep',action='store_true',help='Enable persisted, bounded background reviews');p.add_argument('--train',help='music,architecture');p.add_argument('--resume',action='store_true');p.add_argument('--exit-after-training',action='store_true')
    sub.add_parser('status');sub.add_parser('stop')
    p=sub.add_parser('upkeep');p.add_argument('action',choices=['status','pause','resume'])
    for action in ('cancel','resume'):
        p=sub.add_parser(action);p.add_argument('--instance',required=True)
    p=sub.add_parser('activate');p.add_argument('--domain',choices=['music','architecture','general'],required=True);p.add_argument('--package',required=True)
    p=sub.add_parser('run');p.add_argument('--task',required=True);p.add_argument('--root',default='workshop');p.add_argument('--steps',type=int,default=4)
    try:asyncio.run(main(parser.parse_args()))
    except KeyboardInterrupt:print('Workshop stopped; use --resume to continue checkpointed learning.',file=sys.stderr)
    except Exception as error:print('Workshop error: '+str(error),file=sys.stderr);sys.exit(1)
