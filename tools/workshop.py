#!/usr/bin/env python3
"""Launch C++ runtime/trainer, run curricula, or explicitly activate a package."""
import argparse,asyncio,hashlib,json,os,pathlib,shutil,subprocess,sys,venv
from collections import deque
ROOT=pathlib.Path(__file__).resolve().parents[1]
# The only third-party Python runtime dependency is the WebSocket client.
try:import websockets
except ImportError:
    local=ROOT/'.workshop-venv';python=local/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
    if not python.exists():venv.EnvBuilder(with_pip=True).create(local)
    subprocess.run([str(python),'-m','pip','install','-r',str(ROOT/'tools/requirements.txt')],check=True)
    os.execv(str(python),[str(python),str(pathlib.Path(__file__).resolve()),*sys.argv[1:]])
from curriculum_runner import Runner,atomic,digest,load,publish,event_sink
from ws_client import Client
from diagnostics import settings as trace_settings, Recorder
from process_errors import CommandFailure, exit_description

async def command(*args, log_path=None):
    log=None;process=None;tail=deque(maxlen=12)
    try:
        if log_path:
            log_path=pathlib.Path(log_path);log_path.parent.mkdir(parents=True,exist_ok=True)
            log=log_path.open('ab')
            log.write(('\nCOMMAND '+json.dumps(list(map(str,args)))+'\n').encode('utf-8'));log.flush()
        process=await asyncio.create_subprocess_exec(*map(str,args),cwd=ROOT,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.STDOUT)
        while line:=await process.stdout.readline():
            text=line.decode(errors='replace');print(text,end='',flush=True)
            tail.append(text.rstrip()[-2048:])
            if log:log.write(line);log.flush()
            if event_sink.get():
                event={'type':'workshop.command_output','command':str(args[0]),'text':text}
                try:
                    parsed=json.loads(text)
                    if isinstance(parsed,dict) and parsed.get('type') in ('language.batch','language.epoch','language.stage','language.resumed','language.finished','training.progress'):event=parsed
                except (ValueError,TypeError):pass
                event_sink.get()(event)
        code=await process.wait()
        if log:log.write(('\nPROCESS '+exit_description(code)+'\n').encode('utf-8'));log.flush()
        if code:
            error=CommandFailure(str(args[0]),code,'\n'.join(tail),log_path)
            if event_sink.get():event_sink.get()({'type':'workshop.command_failed','command':str(args[0]),'returncode':code,'exit_status':error.exit_status,'output_tail':error.output_tail,'log_path':error.log_path})
            raise error
    finally:
        if process and process.returncode is None:process.terminate();await process.wait()
        if log:log.close()

def configuration(path, runtime_pack=None):
    if not (ROOT/path).is_file():
        raise ValueError('WDS configuration not found: '+str(ROOT/path)+'. This is generated separately from Six Stars config/models.json. Start with: python tools/workshop.py start --six-stars "U:\\six-stars-solution-2" --ui --upkeep')
    config=load(ROOT/path)
    if 'models' in config or 'packs' in config:
        raise ValueError('This is a Six Stars manifest, not a WDS configuration. Use start --six-stars with its installation or config folder; the launcher generates configs/sixstars.local.json.')
    allowed={'schema','port','build_dir','ggml','llama','llama_source','model','gpu_layers','build_jobs','data','runs',
        'package_root','initial_package','teacher_ws','teacher_model','teacher_port','max_teacher_calls',
        'max_foundation_calls','run_timeout_seconds','trainer_backend','parents','upkeep','cuda','mesh','runtime_pack','cuda_architectures','diagnostics','network_enabled','language'}
    if set(config)-allowed:raise ValueError('unknown configuration keys: '+','.join(sorted(set(config)-allowed)))
    if config.get('schema')!=1:raise ValueError('unknown configuration schema')
    if type(config['port']) is not int or not 1<=config['port']<=65535:raise ValueError('invalid port')
    if not 1<=config.get('build_jobs',4)<=16:raise ValueError('build_jobs must be 1..16')
    if not 1<=config.get('run_timeout_seconds',1800)<=86400:raise ValueError('invalid training time budget')
    if not 0<=config.get('max_teacher_calls',0)<=4 or not 0<=config.get('max_foundation_calls',0)<=100:raise ValueError('model call budget exceeded')
    for key in ('model','teacher_model'):
        if config.get(key) and not (ROOT/config[key]).is_file():raise ValueError(key+' GGUF path does not exist')
    if runtime_pack:
        config['runtime_pack']=str(pathlib.Path(runtime_pack).expanduser().resolve())
    if config.get('runtime_pack'):
        from runtime_pack import configure_pack
        config=configure_pack(config, ROOT/config['runtime_pack'])
    if type(config.get('network_enabled',True)) is not bool:raise ValueError('network_enabled must be boolean')
    language=config.get('language',{})
    if set(language)-{'mode','teacher_calls_per_window','teacher_max_tokens','student_threshold'}:raise ValueError('unknown language settings')
    if language.get('mode','strict') not in ('legacy','strict','assisted'):raise ValueError('invalid language mode')
    if not 0<=language.get('teacher_calls_per_window',8)<=64 or not 32<=language.get('teacher_max_tokens',1024)<=4096:raise ValueError('invalid language teacher budget')
    if not 0<=language.get('student_threshold',.9)<=1:raise ValueError('invalid student threshold')
    trace_settings(config)
    return config

async def build(config):
    cmake=shutil.which('cmake')
    if not cmake:raise RuntimeError('CMake 3.20+ and a C++20 compiler are required. Install them, then rerun this command.')
    directory=(ROOT/config['build_dir']).resolve()
    pack=bool(config.get('runtime_pack'))
    llama=not pack and bool(config.get('llama') or config.get('model') or config.get('teacher_model'))
    options=['-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=OFF','-DWDS_RUNTIME_PACK='+('ON' if pack else 'OFF'),
        '-DGGML_CUDA='+('ON' if config.get('cuda',False) and not pack else 'OFF'),'-DWDS_LLAMA='+('ON' if llama else 'OFF'),
        '-DWDS_GGML_TRAIN='+('ON' if config.get('ggml',True) and not pack else 'OFF'),'-DWDS_LLAMA_SOURCE='+str((ROOT/config['llama_source']).resolve() if config.get('llama_source') else '')]
    if config.get('cuda') and not pack:
        options.append('-DCMAKE_CUDA_ARCHITECTURES='+str(config.get('cuda_architectures','native')))
    h=hashlib.sha256(json.dumps(options).encode())
    for p in sorted([ROOT/'CMakeLists.txt',*list((ROOT/'src').glob('*.cpp')),*list((ROOT/'include').rglob('*.hpp'))]):h.update(p.read_bytes())
    if pack:
        for directory_name in ('llama-b9870','llama-template'):
            for p in sorted((ROOT/'third_party'/directory_name).rglob('*')):
                if p.is_file():h.update(p.read_bytes())
    stamp=directory/'workshop-build.json';same=stamp.exists() and load(stamp).get('fingerprint')==h.hexdigest()
    def binary(name):
        direct=directory/(name+('.exe' if os.name=='nt' else ''))
        return direct if direct.exists() else directory/'Release'/direct.name
    if not same or not all(binary(name).is_file() for name in ('wds-ai','wds-train')):
        publish('workshop.build',ggml=config.get('ggml',True),llama=llama)
        await command(cmake,'-S',ROOT,'-B',directory,*options)
        await command(cmake,'--build',directory,'--config','Release','--target','wds-ai','wds-train','-j',config.get('build_jobs',4))
        atomic(stamp,{'fingerprint':h.hexdigest()})
    return binary('wds-ai'),binary('wds-train')

class Server:
    def __init__(self,binary,config,teacher=False):self.binary=binary;self.config=config;self.teacher=teacher;self.process=None;self.drains=[];self.trace=Recorder(config,'teacher-supervisor' if teacher else 'supervisor')
    async def start(self):
        c=self.config;port=c.get('teacher_port',c['port']+1) if self.teacher else c['port']
        self.endpoint=f'ws://127.0.0.1:{port}'
        data=(ROOT/c['data']/('teacher' if self.teacher else 'runtime')).resolve()
        args=[str(self.binary),'--data',str(data),'--package',str((ROOT/c['initial_package']).resolve()),
            '--packages',str((ROOT/c['package_root']).resolve()),'--port',str(port)]
        model=c.get('teacher_model' if self.teacher else 'model','')
        if model:args+=['--model',str((ROOT/model).resolve()),'--gpu-layers',str(c.get('gpu_layers',0))]
        if c.get('runtime_pack'):args+=['--runtime-pack',str((ROOT/c['runtime_pack']).resolve())]
        language=c.get('language',{})
        args+=['--language-mode',language.get('mode','strict'),'--language-teacher-calls',str(language.get('teacher_calls_per_window',8)),
               '--language-teacher-tokens',str(language.get('teacher_max_tokens',1024)),'--language-student-threshold',str(language.get('student_threshold',.9))]
        options=trace_settings(c)
        if options['enabled']:
            args+=['--trace-file',str(ROOT/c['runs']/'logs'/'diagnostics'/('teacher.jsonl' if self.teacher else 'runtime.jsonl')),
                   '--trace-level',options['level'],'--trace-content',str(options['include_content']).lower(),
                   '--trace-file-mb',str(options['max_file_mb']),'--trace-backups',str(options['keep_files'])]
        self.trace.record('process.start',{'args':args})
        self.process=await asyncio.create_subprocess_exec(*args,cwd=ROOT,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        logdir=(ROOT/c['runs']/'logs');logdir.mkdir(parents=True,exist_ok=True)
        async def drain(stream,path):
            with path.open('ab') as file:
                while line:=await stream.readline():
                    file.write(line);file.flush()
                    self.trace.record('process.output',{'file':path.name,'text':line.decode(errors='replace')},error=b'error' in line.lower() or b'fatal' in line.lower())
        self.drains.append(asyncio.create_task(drain(self.process.stderr,logdir/('teacher.log' if self.teacher else 'runtime.log'))))
        try:ready=await asyncio.wait_for(self.process.stdout.readline(),180)
        except asyncio.TimeoutError:
            raise RuntimeError('Runtime startup exceeded 180 seconds; inspect '+str(logdir)+' for model loading details.') from None
        if not ready.startswith(b'READY'):
            # The child closes stdout when startup fails. Finish draining its diagnostic
            # so DLL/ABI/GPU errors appear in the launcher as well as the retained log.
            await asyncio.wait_for(asyncio.shield(self.drains[0]),5)
            log=logdir/('teacher.log' if self.teacher else 'runtime.log')
            with log.open('rb') as file:
                file.seek(max(0,log.stat().st_size-16384))
                details=file.read().decode(errors='replace')
            raise RuntimeError('Runtime failed to start; log: '+str(log)+'\n'+details)
        self.drains.append(asyncio.create_task(drain(self.process.stdout,logdir/('teacher.stdout.log' if self.teacher else 'runtime.stdout.log'))))
        self.trace.record('process.ready',{'ready':ready.decode(errors='replace')})
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
        try:
            results=await asyncio.wait_for(asyncio.gather(*self.drains,return_exceptions=True),5)
        except asyncio.TimeoutError:
            self.trace.record('log.drain_timeout',{'message':'Final process log drain exceeded five seconds; buffered output may be missing.'},error=True)
            results=await asyncio.gather(*self.drains,return_exceptions=True)
        for result in results:
            if isinstance(result,Exception):self.trace.record("log.drain_failed",{"error":str(result)},error=True)
        self.trace.record("process.stopped",{"returncode":self.process.returncode if self.process else None})
        self.trace.close()

async def build_ui():
    import hashlib
    ui=ROOT/'ui';h=hashlib.sha256()
    for path in sorted([ui/'package.json',ui/'package-lock.json',ui/'index.html',ui/'vite.config.js',*list((ui/'src').rglob('*')),*list((ui/'public').rglob('*'))]):
        if path.is_file():h.update(path.read_bytes())
    stamp=ui/'dist/.source-sha256'
    if stamp.exists() and stamp.read_text()==h.hexdigest():return
    npm=shutil.which('npm.cmd' if os.name=='nt' else 'npm')
    if not npm:raise RuntimeError('Install Node.js 22.12+ to build the React interface, or use the supplied matching ui/dist bundle.')
    await command(npm,'--prefix',ui,'ci','--no-audit','--no-fund')
    await command(npm,'--prefix',ui,'run','build')
    stamp.write_text(h.hexdigest())

async def start(args,config):
    runtime,trainer=await build(config);servers=[];training=None;exits=[];fetch_worker=None;mesh_worker=None;ui_task=None;ui_service=None
    if getattr(args,'ui',False):
        await build_ui()
    try:
        primary=Server(runtime,config);servers.append(primary);await primary.start()
        if config.get('teacher_model'):
            teacher=Server(runtime,config,True);servers.append(teacher);await teacher.start();config={**config,'teacher_ws':teacher.endpoint}
        async with Client(primary.endpoint) as client:
            snapshot=(await client.call('upkeep.inspect','upkeep.snapshot'))['data']
            settings=dict(config.get('upkeep',{}))
            if args.upkeep:settings['enabled']=True
            if settings:await client.call('upkeep.configure','upkeep.configured',expected_version=snapshot['settings']['version'],settings=settings)
            mesh=(await client.call('mesh.inspect','mesh.snapshot',summary=True))['data']
            settings=dict(config.get('mesh',{}))
            settings['web_enabled'] = not getattr(args,'offline',False) and (getattr(args,'web',False) or config.get('network_enabled',True))
            if settings:await client.call('mesh.configure','mesh.configured',expected_version=mesh['settings']['version'],settings=settings)
        from upkeep_worker import run as material_worker
        network_enabled=not getattr(args,'offline',False) and (getattr(args,'web',False) or config.get('network_enabled',True))
        fetch_worker=asyncio.create_task(material_worker(primary.endpoint,network_enabled=network_enabled))
        from mesh_web_worker import run as web_worker
        mesh_worker=asyncio.create_task(web_worker(primary.endpoint))
        exits=[asyncio.create_task(s.process.wait()) for s in servers]
        exits.extend([mesh_worker,fetch_worker])
        if getattr(args,'ui',False):
            from interface import Interface
            ui_service=Interface(config,args.ui_port)
            ui_task=asyncio.create_task(ui_service.run());exits.append(ui_task)
        async def train():
            if getattr(args,'train_english',False):
                from english_workshop import run as train_english
                await train_english(config,'curricula/english-v2.json',args.english_epochs,None)
            for domain in (args.train or '').split(','):
                if not domain:continue
                if domain not in ('music','architecture'):raise ValueError('unknown curriculum: '+domain)
                if ui_service:
                    ui_service.job={'status':'running','domain':domain};ui_service.publish(ui_service.snapshot())
                    ui_service.task=asyncio.create_task(ui_service.train(domain))
                    try:await ui_service.task
                    except asyncio.CancelledError:
                        if ui_service.job.get('status')=='cancelled':return
                        raise
                    if ui_service.job['status']!='completed':raise RuntimeError(ui_service.job.get('error','training failed'))
                    continue
                runner=Runner(config,domain,primary.endpoint,trainer,args.resume)
                try:await asyncio.wait_for(runner.run_all(),config['run_timeout_seconds'])
                except asyncio.TimeoutError:raise RuntimeError('Training time budget expired; checkpoints remain in '+str(ROOT/config['runs'])+'; rerun start --train '+args.train+' --resume')
        if args.train or getattr(args,'train_english',False):
            training=asyncio.create_task(train())
            if ui_service:ui_service.external_training=training
            done,_=await asyncio.wait([training,fetch_worker,*exits],return_when=asyncio.FIRST_COMPLETED)
            if fetch_worker in done:await fetch_worker;raise RuntimeError('material worker stopped')
            if training not in done:raise RuntimeError('runtime exited during training')
            await training
            if args.exit_after_training:return
            publish('workshop.idle',message='Training finished. Runtime remains available; activation is a separate command.')
        done,_=await asyncio.wait([*exits,fetch_worker],return_when=asyncio.FIRST_COMPLETED)
        if fetch_worker in done:await fetch_worker;raise RuntimeError('material worker stopped')
        if any(task.result()!=0 for task in done):raise RuntimeError('runtime process failed; see run logs')
    except Exception as error:
        if servers:servers[0].trace.record('workshop.failed',{'error':str(error)},error=True)
        raise
    finally:
        if ui_task:ui_task.cancel();await asyncio.gather(ui_task,return_exceptions=True)
        if fetch_worker:fetch_worker.cancel();await asyncio.gather(fetch_worker,return_exceptions=True)
        if mesh_worker:mesh_worker.cancel();await asyncio.gather(mesh_worker,return_exceptions=True)
        if training and not training.done():training.cancel();await asyncio.gather(training,return_exceptions=True)
        for server in reversed(servers):await server.stop()
        for task in exits:
            if not task.done():task.cancel()

async def main(args):
    if args.command=='start' and args.six_stars:
        from configure_sixstars import configure
        args.config=str(configure(argparse.Namespace(six_stars=args.six_stars,models_root=args.models_root,model=args.model,
            cuda=args.cuda,gpu_layers=args.gpu_layers,llama_source=None,output=args.config or 'configs/sixstars.local.json',
            replace=False,reuse=True,port=8765,runtime_pack=getattr(args,'runtime_pack',None))))
    elif args.command=='start' and (args.models_root or args.model or args.cuda or args.gpu_layers is not None):
        raise ValueError('--models-root, --model, --cuda and --gpu-layers require --six-stars during setup; otherwise use --config.')
    args.config=args.config or 'configs/local.json'
    config=configuration(args.config,getattr(args,'runtime_pack',None) if args.command=='start' else None)
    if args.command=='start' and getattr(args,'runtime_pack',None):
        from runtime_pack import configure_pack
        config=configure_pack(config,args.runtime_pack)
        atomic(ROOT/args.config,config)
        publish('workshop.runtime_pack',directory=config['runtime_pack'],build_dir=config['build_dir'],
            gpu_layers=config.get('gpu_layers',0),message='Existing model and data paths preserved; CUDA compilation disabled.')
    if args.command=='start' and getattr(args,'trace',False):
        config['diagnostics']={**trace_settings(config),'enabled':True,'level':'trace','include_content':True}
        atomic(ROOT/args.config,config)
    endpoint=f"ws://127.0.0.1:{config['port']}"
    if args.command=='start' and getattr(args,'language_mode',None):
        config['language']={**config.get('language',{}),'mode':args.language_mode}
    if args.command=='start':
        trace=Recorder(config,'workshop')
        token=event_sink.set(lambda event:trace.record('workshop.event',event))
        try:
            trace.record('workshop.start',{'config':str(args.config),'ui':getattr(args,'ui',False),'train':getattr(args,'train',None)})
            return await start(args,config)
        except Exception as error:
            trace.record('workshop.failed',{'error':str(error)},error=True);raise
        finally:
            trace.record('workshop.stopped');event_sink.reset(token);trace.close()
    if args.command=='english-train':
        from english_workshop import run
        return await run(config,args.curriculum,args.epochs,args.output,args.root)
    if args.command=='english-export':
        from english_workshop import export
        return await export(config,args.root,args.output)
    if args.command=='language-train':
        from language_workshop import run
        return await run(config,args.root,args.curriculum)
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
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--config',help='WDS config; defaults to configs/local.json, or configs/sixstars.local.json with --six-stars')
    sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('start');p.add_argument('--ui',action='store_true',help='Build and serve the local React workspace');p.add_argument('--ui-port',type=int,default=8787);p.add_argument('--upkeep',action='store_true',help='Enable persisted, bounded background reviews');p.add_argument('--train',help='music,architecture');p.add_argument('--resume',action='store_true');p.add_argument('--exit-after-training',action='store_true')
    p.add_argument('--train-english',action='store_true',help='Train an English sequence candidate while the app runs; never activates it');p.add_argument('--english-epochs',type=int,default=8)
    p.add_argument('--language-mode',choices=['assisted','strict','legacy'],help='assisted: explicit streaming Qwen backup; strict: sequence student only')
    p.add_argument('--trace',action='store_true',help='Persist full local diagnostics including message content; export with tools/diagnostics.py')
    p.add_argument('--web',action='store_true',help='Enable bounded Wikipedia search (on by default unless network_enabled=false)')
    p.add_argument('--offline',action='store_true',help='Disable internet acquisition; preserve access to local archives')
    p.add_argument('--six-stars',help='Generate/reuse WDS config from the Six Stars install or config folder')
    p.add_argument('--models-root');p.add_argument('--model');p.add_argument('--cuda',action='store_true');p.add_argument('--gpu-layers',type=int)
    p.add_argument('--runtime-pack',help='Existing b9870 native library folder; uses DLLs without building CUDA')
    sub.add_parser('status');sub.add_parser('stop')
    p=sub.add_parser('english-train');p.add_argument('--curriculum',default='curricula/english-v2.json');p.add_argument('--epochs',type=int,default=8);p.add_argument('--output');p.add_argument('--root',help='Include only explicitly approved lessons from this root')
    p=sub.add_parser('english-export');p.add_argument('--root',required=True);p.add_argument('--output',required=True)
    p=sub.add_parser('language-train');p.add_argument('--root',default='language-workshop');p.add_argument('--curriculum',default='curricula/language-v1.json')
    p=sub.add_parser('upkeep');p.add_argument('action',choices=['status','pause','resume'])
    for action in ('cancel','resume'):
        p=sub.add_parser(action);p.add_argument('--instance',required=True)
    p=sub.add_parser('activate');p.add_argument('--domain',choices=['music','architecture','general','neural','expression','language'],required=True);p.add_argument('--package',required=True)
    p=sub.add_parser('run');p.add_argument('--task',required=True);p.add_argument('--root',default='workshop');p.add_argument('--steps',type=int,default=4)
    try:asyncio.run(main(parser.parse_args()))
    except KeyboardInterrupt:print('Workshop stopped; use --resume to continue checkpointed learning.',file=sys.stderr)
    except Exception as error:print('Workshop error: '+str(error),file=sys.stderr);sys.exit(1)
