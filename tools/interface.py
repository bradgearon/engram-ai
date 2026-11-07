#!/usr/bin/env python3
"""Loopback-only UI delivery, WebSocket relay and native training supervision."""
import argparse, asyncio, contextlib, json, mimetypes, pathlib, sys, uuid, time
from http import HTTPStatus
from urllib.parse import unquote, urlsplit
import websockets
from websockets.asyncio.server import serve
from websockets.http11 import Response
from websockets.datastructures import Headers
from curriculum_runner import Runner, ROOT, atomic, load, event_sink
from workshop import configuration
from diagnostics import Recorder, settings as trace_settings

class Interface:
    def __init__(self, config, port=8787, dev=False):
        self.trace=Recorder(config,"interface")
        self.config=config; self.port=port; self.dev=dev; self.clients=set(); self.task=None
        self.state_path=ROOT/config['runs']/'interface-job.json'
        self.job=load(self.state_path) if self.state_path.exists() else {'status':'idle'}
        if self.job.get('status')=='running':
            self.job.update(status='interrupted',error='Supervisor stopped. Resume training from its checkpoint.')
            atomic(self.state_path,self.job)
        self.events=[]; self.endpoint=f"ws://127.0.0.1:{config['port']}"
    def origin_allowed(self, origin):
        if not origin:return False
        p=urlsplit(origin)
        return p.scheme=='http' and p.hostname in ('localhost','127.0.0.1') and p.port==self.port
    async def http(self, connection, request):
        host=request.headers.get('Host','').split(':')[0]
        if host not in ('localhost','127.0.0.1') and not self.dev:
            return connection.respond(HTTPStatus.FORBIDDEN,'Local host required')
        if request.path in ('/runtime','/workshop','/diagnostics'):
            if not self.dev and not self.origin_allowed(request.headers.get('Origin')):
                return connection.respond(HTTPStatus.FORBIDDEN,'Same-origin UI connection required')
            return None
        base=(ROOT/'ui/dist').resolve()
        path=(base/unquote(urlsplit(request.path).path).lstrip('/')).resolve()
        if not path.is_relative_to(base):return connection.respond(HTTPStatus.FORBIDDEN,'Invalid path')
        if not path.is_file():path=base/'index.html'
        if not path.is_file():return connection.respond(HTTPStatus.SERVICE_UNAVAILABLE,'Build the interface: cd ui && npm ci && npm run build')
        body=path.read_bytes()
        headers=Headers({'Content-Type':mimetypes.guess_type(path.name)[0] or 'application/octet-stream',
            'Content-Length':str(len(body)),'Cache-Control':'no-cache','X-Content-Type-Options':'nosniff',
            'Referrer-Policy':'no-referrer','Content-Security-Policy':"default-src 'self'; connect-src 'self' ws://127.0.0.1:* ws://localhost:*; style-src 'self' 'unsafe-inline'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'self'; object-src 'none'"})
        return Response(200,'OK',headers,body)
    def snapshot(self):
        curricula=[]
        for domain in ('music','architecture'):
            manifest=load(ROOT/'curricula'/domain/'manifest.json')
            curricula.append({'domain':domain,'manifest':manifest,'sample':load(ROOT/'curricula'/domain/manifest['splits']['train']['path'])[0]['task']})
        return {'type':'training.snapshot','job':self.job,'events':self.events[-100:],'curricula':curricula}
    def publish(self,event):
        self.trace.record("training.event",event,error=event.get("job",{}).get("status")=="failed")
        self.events.append(event);self.events=self.events[-200:]
        websockets.broadcast(self.clients,json.dumps(event))
    async def train(self, domain):
        def capture(event):self.publish(event)
        token=event_sink.set(capture)
        try:
            directory=ROOT/self.config['build_dir']
            trainer=directory/('wds-train.exe' if sys.platform=='win32' else 'wds-train')
            if not trainer.exists():trainer=directory/'Release'/trainer.name
            if not trainer.is_file():raise ValueError('Native trainer missing; start via workshop.py --ui to build it.')
            if domain == 'english':
                from english_workshop import run
                report=await asyncio.wait_for(run(self.config,root=self.job.get('root_id')),self.config.get('run_timeout_seconds',1800))
                self.job.update(status='completed',candidate=report['package'],report=report)
            elif domain == 'language':
                from language_workshop import run
                report=await asyncio.wait_for(run(self.config,self.job['root_id']),self.config.get('run_timeout_seconds',1800))
                self.job.update(status='completed',candidate=report['package'],report=report)
            elif domain in ('neural','expression'):
                from neural_workshop import train
                candidate,report=await asyncio.wait_for(train(self.config,self.endpoint,trainer,self.job['root_id'],domain),self.config.get('run_timeout_seconds',1800))
                self.job.update(status='completed',candidate=str(candidate.relative_to(ROOT)),report=report)
            else:
                runner=Runner(self.config,domain,self.endpoint,trainer,True)
                self.job.update(run_id=runner.identity);atomic(self.state_path,self.job)
                await asyncio.wait_for(runner.run_all(),self.config.get('run_timeout_seconds',1800))
                self.job.update(status='completed',candidate=str(runner.candidate.relative_to(ROOT)),report=runner.state.get('report',{}))
        except asyncio.CancelledError:
            self.job.update(status='cancelled',error='Training stopped; checkpoints retained. Resume to continue.')
            raise
        except Exception as error:
            self.job.update(status='failed',error=str(error) or type(error).__name__)
            if hasattr(error,'returncode'):
                self.job.update(returncode=error.returncode,exit_status=error.exit_status,log_path=error.log_path)
        finally:
            event_sink.reset(token);atomic(self.state_path,self.job);self.publish(self.snapshot())
    async def handle(self,ws):
        if ws.request.path=='/diagnostics':
            options=trace_settings(self.config)
            await ws.send(json.dumps({'type':'diagnostics.config','settings':options}))
            window=time.monotonic(); count=0; dropped=0
            try:
                async for raw in ws:
                    if not options['enabled']:continue
                    if time.monotonic()-window>=1:
                        if dropped:self.trace.record('browser.rate_limit',{'dropped':dropped},error=True)
                        window=time.monotonic();count=0;dropped=0
                    count+=1
                    if count>500:dropped+=1;continue
                    try:
                        row=json.loads(raw)
                        if not isinstance(row,dict) or not isinstance(row.get('kind'),str):raise ValueError('invalid browser trace')
                        self.trace.record('browser.event',row,error=bool(row.get('error')))
                    except Exception as error:self.trace.record('browser.invalid_trace',{'error':str(error)},error=True)
            finally:
                if dropped:self.trace.record('browser.rate_limit',{'dropped':dropped},error=True)
            return
        self.trace.record('connection.open',{'path':ws.request.path})
        if ws.request.path=='/runtime':
            try:
                async with websockets.connect(self.endpoint,origin=None,ping_interval=None,max_size=524288) as runtime:
                    async def relay(src,dst):
                        async for message in src:
                            # Native trace captures responses, including background events with no UI connected.
                            if src is ws:self.trace.record('relay.request',{'raw':message})
                            await dst.send(message)
                    tasks=[asyncio.create_task(relay(ws,runtime)),asyncio.create_task(relay(runtime,ws))]
                    try:
                        done,_=await asyncio.wait(tasks,return_when=asyncio.FIRST_COMPLETED)
                        for t in done:t.result()
                    finally:
                        for t in tasks:t.cancel()
                        await asyncio.gather(*tasks,return_exceptions=True)
            except Exception as error:
                self.trace.record("relay.failed",{"error":str(error)},error=True)
                with contextlib.suppress(Exception):await ws.send(json.dumps({'type':'error','message':'Runtime connection lost: '+str(error)}));await ws.close(1011)
            return
        if ws.request.path!='/workshop':await ws.close(1008);return
        self.clients.add(ws)
        try:
            await ws.send(json.dumps(self.snapshot()))
            async for raw in ws:
                try:
                    m=json.loads(raw)
                    if m.get('type')=='training.start':
                        if (self.task and not self.task.done()) or (getattr(self,'external_training',None) and not self.external_training.done()):raise ValueError('One training job is already running')
                        domain=m.get('domain')
                        if domain not in ('music','architecture','neural','expression','language','english'):raise ValueError('Unknown curriculum')
                        if domain in ('neural','expression','language','english'):
                            root=m.get('root_id')
                            if not isinstance(root,str) or not 0<len(root)<=80:raise ValueError('Choose a root for neuronal feedback training')
                        self.job={'id':uuid.uuid4().hex,'domain':domain,'status':'running'}
                        if domain in ('neural','expression','language','english'):self.job['root_id']=root
                        atomic(self.state_path,self.job)
                        self.publish(self.snapshot());self.task=asyncio.create_task(self.train(domain))
                    elif m.get('type')=='training.cancel':
                        if self.task and not self.task.done():self.task.cancel();await asyncio.gather(self.task,return_exceptions=True)
                    else:raise ValueError('Unknown training command')
                except Exception as error:
                    self.trace.record('training.command_failed',{'error':str(error),'raw':raw},error=True)
                    await ws.send(json.dumps({'type':'error','message':str(error)}))
        finally:self.clients.discard(ws)
    async def run(self):
        async with serve(self.handle,'127.0.0.1',self.port,process_request=self.http,max_size=16384,ping_interval=None):
            print('INTERFACE http://127.0.0.1:'+str(self.port),flush=True)
            try:await asyncio.Future()
            finally:
                if self.task and not self.task.done():self.task.cancel();await asyncio.gather(self.task,return_exceptions=True)
                self.trace.record("interface.stopped")
                self.trace.close()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--config',default='configs/offline.json');p.add_argument('--port',type=int,default=8787)
    p.add_argument('--dev',action='store_true',help='Allow the development preview proxy; loopback binding remains mandatory')
    args=p.parse_args()
    try:asyncio.run(Interface(configuration(args.config),args.port,args.dev).run())
    except KeyboardInterrupt:pass
