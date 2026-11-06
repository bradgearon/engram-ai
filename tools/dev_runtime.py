"""Development-only runtime and UI bridge; uses an explicit local config."""
import argparse,asyncio,signal
from workshop import Server,configuration,ROOT
from interface import Interface
async def main(args):
    config=configuration(args.config)
    loop=asyncio.get_running_loop();task=asyncio.current_task()
    for sig in (signal.SIGTERM,signal.SIGINT):
        try:loop.add_signal_handler(sig,task.cancel)
        except NotImplementedError:pass
    binary=ROOT/config['build_dir']/('wds-ai.exe' if __import__('os').name=='nt' else 'wds-ai')
    if not binary.exists():binary=binary.parent/'Release'/binary.name
    server=Server(binary,config)
    try:
        await server.start()
        await Interface(config,8787,True).run()
    finally:await server.stop()
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--config',required=True)
    try:asyncio.run(main(p.parse_args()))
    except KeyboardInterrupt:pass
