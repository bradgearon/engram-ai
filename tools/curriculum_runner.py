"""Curriculum I/O, WebSocket episodes and native trainer supervision. No training math."""
import asyncio,hashlib,json,os,pathlib,shutil,time,uuid
from ws_client import Client
ROOT=pathlib.Path(__file__).resolve().parents[1]

def digest(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def atomic(path,value):
    path=pathlib.Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_name(path.name+'.tmp')
    with temp.open('w') as f:json.dump(value,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
    os.replace(temp,path)
def load(path):return json.loads(pathlib.Path(path).read_text())
def publish(kind,**fields):print(json.dumps({'type':kind,**fields}),flush=True)
def stable(*parts):return hashlib.sha256('|'.join(map(str,parts)).encode()).hexdigest()[:40]

class Runner:
    def __init__(self,config,domain,endpoint,trainer,resume=False):
        self.config=config;self.domain=domain;self.endpoint=endpoint;self.trainer=trainer
        self.folder=ROOT/'curricula'/domain;self.manifest=load(self.folder/'manifest.json')
        if self.manifest['domain']!=domain:raise ValueError('curriculum domain mismatch')
        self.tasks={};self.pins={}
        families={}
        for split,spec in self.manifest['splits'].items():
            path=(self.folder/spec['path']).resolve()
            if not path.is_relative_to(self.folder.resolve()) or digest(path)!=spec['sha256']:raise ValueError('curriculum split changed')
            self.tasks[split]=load(path);self.pins[split]=digest(path)
            families[split]={x['family'] for x in self.tasks[split]}
        if any(families[a]&families[b] for a,b in [('train','validation'),('train','test'),('validation','test')]):raise ValueError('task family leakage')
        for source in self.manifest['sources']:
            path=(self.folder/source['path']).resolve()
            if not path.is_relative_to(self.folder.resolve()) or digest(path)!=source['sha256']:raise ValueError('lesson source changed')
        self.parent=(ROOT/config.get('parents',{}).get(domain,config['initial_package'])).resolve()
        parent_manifest=load(self.parent/'manifest.json')
        implementation=hashlib.sha256()
        for source in sorted([ROOT/'CMakeLists.txt',*list((ROOT/'src').glob('*.cpp')),
                *list((ROOT/'include').rglob('*.hpp')),*list((ROOT/'tools').glob('*.py'))]):
            implementation.update(source.relative_to(ROOT).as_posix().encode());implementation.update(source.read_bytes())
        self.implementation=implementation.hexdigest()
        self.identity=stable(self.implementation,json.dumps(self.manifest,sort_keys=True),digest(self.parent/'policy.wind'),
            digest(self.parent/'manifest.json'),digest(trainer),config.get('trainer_backend','auto'),config.get('teacher_ws',''),
            config.get('max_teacher_calls',0),config.get('max_foundation_calls',0))
        self.run=(ROOT/config['runs']/domain/self.identity).resolve();self.run.mkdir(parents=True,exist_ok=True)
        self.file=self.run/'run.json';self.state=load(self.file) if self.file.exists() else {
            'schema':1,'id':self.identity,'domain':domain,'pins':self.pins,'identity':self.identity,
            'implementation_sha256':self.implementation,'created_ms':int(time.time()*1000),'imported':[],'trajectories':{},'results':{},'teacher':[], 'status':'pending'}
        if self.state['identity']!=self.identity or self.state['pins']!=self.pins:raise ValueError('run fingerprint conflict')
        if self.file.exists() and not resume:raise ValueError(f'run already exists; use --resume: {self.run}')
        self.root='learn-'+domain
        self.candidate=(ROOT/config['package_root']/'candidates'/domain/self.identity).resolve()
        self.checkpoint()
    def checkpoint(self):atomic(self.file,self.state)
    async def import_lessons(self,client):
        import learn
        # Explicit, bounded source imports. IDs are stable and current versions are inspected once on resume.
        for source in self.manifest['sources']:
            if '/extracted/' not in source['path']:continue
            source_id=stable(source['path'],source['sha256'])
            if source_id in self.state['imported']:continue
            parts=learn.chunks((self.folder/source['path']).read_text())
            for i,part in enumerate(parts):
                packet_id='lesson-'+source_id+'-'+str(i)
                try:
                    await client.call('memory.upsert','memory.accepted',request=packet_id,packet={
                        'schema':1,'intent':'memory.upsert','hops':0,'root_id':self.root,'engram_id':packet_id,
                        'packet_id':packet_id,'expected_version':0,'expires_ms':int(time.time()*1000)+60000,
                        'text':part,'source':self.domain+':'+source['path']+'#sha256='+source['sha256'],'claim_status':'hypothetical'})
                except RuntimeError as error:
                    if 'duplicate packet' not in str(error):raise
            self.state['imported'].append(source_id);self.checkpoint()
    async def trajectories(self,client,split):
        rows=[]
        for fixture in self.tasks[split]:
            task=fixture['task'];key=split+'/'+task['id']
            if key not in self.state['trajectories']:
                trace=[];current=task
                for step in range(self.manifest['max_trajectory_steps']):
                    response=await client.call('practice.assess','practice.options',root_id=self.root,task=current)
                    data=response['data'];trace.append({'features':data['features'][:16],'target':data['target_action'],
                        'task_id':task['id'],'family':fixture['family'],'step':step,'score':data['score'],
                        'options':data['options'],'evaluator_id':data['evaluator_id']})
                    if data['target_action']==6:break
                    current=data['next_task']
                self.state['trajectories'][key]=trace;self.checkpoint()
            rows.extend(self.state['trajectories'][key])
        return rows
    async def native_train(self,train,validation):
        job={'schema':1,'domain':self.domain,'parent':str(self.parent),'output':str(self.candidate),
            'train':train,'validation':validation,'epochs':self.manifest['training_epochs'],
            'learning_rate':self.manifest['learning_rate'],'split_fingerprints':self.pins}
        if self.config.get('trainer_backend','auto')!='auto':job['backend']=self.config['trainer_backend']
        path=self.run/'training_job.json'
        if path.exists() and load(path)!=job:raise ValueError('immutable training job changed')
        atomic(path,job)
        process=await asyncio.create_subprocess_exec(str(self.trainer),'--job',str(path),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        async def errors():
            data=await process.stderr.read();return data.decode(errors='replace')[-12000:]
        stderr=asyncio.create_task(errors())
        try:
            while line:=await process.stdout.readline():
                event=json.loads(line);publish(event.pop('type'),domain=self.domain,**event)
            code=await process.wait();error=await stderr
            if code:raise RuntimeError('native trainer failed: '+error)
        finally:
            if process.returncode is None:process.terminate();await process.wait()
            if not stderr.done():stderr.cancel()
    async def register(self,client,path):
        manifest=load(path/'manifest.json')
        relative=path.relative_to((ROOT/self.config['package_root']).resolve()).as_posix()
        result=await client.call('package.register','package.available',path=relative,checksum=manifest['checksum_fnv1a64'])
        return result['data']['key']
    async def benchmark(self,client,split,keys):
        modes=['parent','fixed_refine','input_once','candidate']
        real='demo' not in client.hello.get('foundation','demo')
        if real and self.config.get('max_foundation_calls',0)>0:modes.append('foundation_once')
        for mode in modes:
            for fixture in self.tasks[split]:
                task=fixture['task'];key=split+'/'+mode+'/'+task['id']
                if key in self.state['results']:continue
                if mode=='foundation_once':
                    count=sum('/foundation_once/' in k for k in self.state['results'])
                    if count>=self.config['max_foundation_calls']:break
                request=stable(self.identity,key)
                result=await client.episode(request,root_id=self.root,text='Practice '+self.domain+' constraints.',task=task,
                    package_key=keys['candidate' if mode=='candidate' else 'parent'],mode='wind' if mode in ('parent','candidate') else mode,
                    max_steps=self.manifest['max_steps'],allow_upkeep=False,work_class="workshop")
                ev=result.get('evaluation',{})
                self.state['results'][key]={'status':result['status'],'score':ev.get('score',0),
                    'passed':result['status']=='completed' and ev.get('passed',False),'evaluation':ev,
                    'artifact':result.get('artifact',''),'request_id':request,'error':result.get('error','')}
                self.checkpoint();publish('benchmark.task',domain=self.domain,split=split,mode=mode,task=task['id'],status=result['status'])
        summary={}
        for mode in modes:
            values=[v for k,v in self.state['results'].items() if k.startswith(split+'/'+mode+'/')]
            if values:summary[mode]={'tasks':len(values),'passes':sum(v['passed'] for v in values),
                'mean_constraint_score':sum(v['score'] for v in values)/len(values),
                'failures':sum(v['status']=='failed' for v in values),'max_actions_per_task':1 if mode in ('input_once','foundation_once') else self.manifest['max_steps']}
        return summary
    async def teacher(self):
        endpoint=self.config.get('teacher_ws','')
        if not endpoint or len(self.state['teacher'])>=self.config.get('max_teacher_calls',0):return
        # Never expose held-out task inputs or labels to the teacher.
        async with Client(endpoint) as teacher:
            if 'demo' in teacher.hello.get('foundation','demo'):raise ValueError('configured teacher must use a real model')
            request=stable(self.identity,'teacher',len(self.state['teacher']))
            self.state['teacher'].append({'request_id':request,'status':'requested','source':teacher.hello.get('foundation')});self.checkpoint()
            await teacher.send({'type':'seed','request_id':request,'root_id':'teacher-curriculum',
                'text':'Provide a short critique and counterexamples for '+self.domain+' repair practice. Do not request tools. No held-out tasks are supplied.','max_steps':1})
            result=await teacher.receive({'episode.finished','wind.checkpoint'},request)
            while result['type']=='wind.checkpoint' and result['data']['status']!='failed':
                result=await teacher.receive({'episode.finished','wind.checkpoint'},request)
            self.state['teacher'][-1].update(status='recorded',artifact=result['data'].get('artifact',''),role='unverified critique; excluded from numerical labels')
            self.checkpoint()
    async def run_all(self):
        self.state['status']='running';self.checkpoint()
        async with Client(self.endpoint) as client:
            await client.call('root.select','root.selected',root_id=self.root,purpose='Practice '+self.domain+' with auditable task evidence.')
            parent_key=await self.register(client,self.parent)
            before=(await client.call('package.list','package.snapshot'))['data']['active']
            self.state['active_at_start']=self.state.get('active_at_start',before);self.checkpoint()
            await self.import_lessons(client);await self.teacher()
            training=await self.trajectories(client,'train');validation=await self.trajectories(client,'validation')
            await self.native_train(training,validation)
            candidate_key=await self.register(client,self.candidate);keys={'parent':parent_key,'candidate':candidate_key}
            validation_result=await self.benchmark(client,'validation',keys)
            # Selection is complete before the first locked-test episode. Test feedback never returns to training.
            test_result=await self.benchmark(client,'test',keys)
            def gate(summary):
                c=summary['candidate'];p=summary['parent'];b=summary['fixed_refine']
                return c['failures']==0 and c['mean_constraint_score']>p['mean_constraint_score']+1e-6 and c['mean_constraint_score']>b['mean_constraint_score']+1e-6
            report={'schema':1,'run_id':self.identity,'domain':self.domain,'candidate_key':candidate_key,'parent_key':parent_key,
                'implementation_sha256':self.implementation,'curriculum_sha256':digest(self.folder/'manifest.json'),'split_pins':self.pins,
                'validation':validation_result,'locked_test':test_result,'accepted':gate(validation_result) and gate(test_result),
                'gate':'candidate improves mean declared-constraint score over parent and fixed-order refinement at equal action limits; no candidate execution failures',
                'scope':self.manifest['scope'],'model_identity':client.hello['foundation'],
                'input_once_reference':'unchanged input fixture; not an LLM baseline',
                'foundation_reference':'not run' if 'foundation_once' not in test_result else 'separate bounded reference; not matched-compute gate',
                'activation':'not requested; this runner never activates packages','package':str(self.candidate.relative_to(ROOT))}
            atomic(self.run/'report.json',report)
            (self.run/'LEARNING_REPORT.md').write_text('# '+self.domain+' Wind learning report\n\n'+json.dumps(report,indent=2)+'\n')
            await client.call('learning.record','learning.candidate',root_id=self.root,report=report)
            self.state['status']='finished';self.state['report']=report;self.checkpoint()
            publish('workshop.finished',domain=self.domain,accepted=report['accepted'],package=report['package'],report=str(self.run/'report.json'),activated=False)
            return report
