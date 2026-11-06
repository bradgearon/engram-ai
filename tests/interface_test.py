"""Same-origin relay, static asset boundary and durable browser-independent chat."""
import asyncio,json,pathlib,socket,sys,tempfile,contextlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from interface import Interface
from websockets.asyncio.server import serve
import websockets
from ws_client import Client
def port():
    with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
async def test(binary,package):
    with tempfile.TemporaryDirectory() as folder:
        rp,up=port(),port()
        proc=await asyncio.create_subprocess_exec(binary,'--data',folder+'/chain','--package',package,'--port',str(rp),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        try:
            assert (await proc.stdout.readline()).startswith(b'READY')
            config={'port':rp,'runs':folder+'/runs','build_dir':'build-v04'};app=Interface(config,up)
            async with serve(app.handle,'127.0.0.1',up,process_request=app.http,max_size=16384,ping_interval=None):
                try:
                    async with websockets.connect(f'ws://127.0.0.1:{up}/runtime',origin='https://unrelated.invalid'):raise AssertionError('foreign origin accepted')
                except websockets.exceptions.InvalidStatus:pass
                reader,writer=await asyncio.open_connection('127.0.0.1',up)
                writer.write(f'GET /%2e%2e/tools/interface.py HTTP/1.1\r\nHost: 127.0.0.1:{up}\r\nConnection: close\r\n\r\n'.encode());await writer.drain()
                assert b'403' in await reader.readline();writer.close();await writer.wait_closed()
                async with websockets.connect(f'ws://127.0.0.1:{up}/runtime',origin=f'http://127.0.0.1:{up}') as ws:
                    assert json.loads(await ws.recv())['type']=='hello'
                    async def call(kind,expected,**values):
                        await ws.send(json.dumps({'schema':1,'type':kind,'request_id':kind.replace('.','-'),**values}))
                        while True:
                            e=json.loads(await ws.recv())
                            assert e['type']!='error',e
                            if e['type']==expected:return e
                    await call('project.create','project.saved',project_id='test',name='Test',purpose='music composition')
                    await call('chat.create','chat.saved',chat_id='chat',project_id='test',title='Test chat')
                    await call('chat.send','chat.message',chat_id='chat',text='music composition with breathing room')
                async with Client(f'ws://127.0.0.1:{rp}') as client:
                    await client.call('upkeep.watch','upkeep.snapshot')
                    state=(await client.call('chat.history','chat.history',chat_id='chat'))['data']
                    if len(state['messages'])<2:
                        await client.receive({'chat.message'})
                        state=(await client.call('chat.history','chat.history',chat_id='chat'))['data']
                    assert len(state['messages'])==2 and state['messages'][-1]['role']=='assistant'
                    await client.send({'type':'runtime.shutdown','request_id':'stop'})
            await asyncio.wait_for(proc.wait(),10)
        finally:
            if proc.returncode is None:proc.terminate();await proc.wait()
    print('same-origin relay, traversal rejection and browser-independent chat passed')
if __name__=='__main__':asyncio.run(test(*sys.argv[1:]))
