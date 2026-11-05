#!/usr/bin/env python3
"""URL/text imports with source provenance; policy training lives in native wds-train."""
import argparse, asyncio, hashlib, ipaddress, json, pathlib, socket, ssl, time, uuid
from html.parser import HTMLParser
from urllib.parse import urlsplit, urljoin
from http.client import HTTPSConnection
import websockets

class Text(HTMLParser):
    def __init__(self): super().__init__(); self.hidden=0; self.parts=[]
    def handle_starttag(self,tag,attrs):
        if tag in ('script','style','noscript'): self.hidden+=1
        elif tag in ('p','div','li','h1','h2','h3','br'): self.parts.append('\n')
    def handle_endtag(self,tag):
        if tag in ('script','style','noscript') and self.hidden: self.hidden-=1
        elif tag in ('p','div','li'): self.parts.append('\n')
    def handle_data(self,data):
        if not self.hidden:self.parts.append(data)

class PinnedHTTPS(HTTPSConnection):
    def __init__(self,host,ip):super().__init__(host,timeout=20,context=ssl.create_default_context());self.ip=ip
    def connect(self):
        raw=socket.create_connection((self.ip,443),self.timeout)
        self.sock=self._context.wrap_socket(raw,server_hostname=self.host)

def fetch(url):
    # Resolve/check every redirect, then connect to the checked IP with original TLS hostname.
    for _ in range(4):
        u=urlsplit(url)
        if u.scheme!='https' or not u.hostname or u.username or u.password or u.port not in (None,443):raise ValueError('public HTTPS URLs only')
        addresses={a[4][0] for a in socket.getaddrinfo(u.hostname,443,type=socket.SOCK_STREAM)}
        if not addresses or any(not ipaddress.ip_address(a).is_global for a in addresses):raise ValueError('non-public URL address blocked')
        conn=PinnedHTTPS(u.hostname,sorted(addresses)[0])
        try:
            conn.request('GET',(u.path or '/')+('?' +u.query if u.query else ''),headers={'User-Agent':'wds.ai.learning/0.1','Accept':'text/html,text/plain','Accept-Encoding':'identity'})
            r=conn.getresponse()
            if r.status in (301,302,303,307,308):url=urljoin(url,r.getheader('Location',''));continue
            if r.status!=200:raise ValueError(f'HTTP {r.status}')
            kind=r.getheader('Content-Type','').split(';')[0]
            if kind not in ('text/html','text/plain'):raise ValueError('only HTML/plain text supported')
            data=r.read(1024*1024+1)
            if len(data)>1024*1024:raise ValueError('page exceeds 1 MiB')
            text=data.decode('utf-8',errors='replace')
            if kind=='text/html':parser=Text();parser.feed(text);text=' '.join(parser.parts)
            return url,' '.join(text.split()),hashlib.sha256(data).hexdigest()
        finally:conn.close()
    raise ValueError('redirect limit exceeded')

def chunks(text):
    # Byte limit respected for multilingual text too. No web scripts or embedded instructions executed.
    result=[];part=''
    for word in text.split():
        if len(word.encode())>1500:continue
        if len((part+' '+word).encode())>1500:
            if part:result.append(part)
            part=word
        else:part=(part+' '+word).strip()
        if len(result)>=32:break
    if part and len(result)<32:result.append(part)
    return result

def train(parts,base,out,source):
    """Compatibility entry point: URL imports are memory; native curricula train Winds."""
    return {'status':'memory_only','accepted':False,'source':source,
            'reason':'Use workshop structured curricula and native wds-train for policy learning.'}

async def exchange(ws,message,expected):
    await ws.send(json.dumps(message))
    while True: # blocking receive, no timer/poll loop
        msg=json.loads(await asyncio.wait_for(ws.recv(),30))
        if msg.get('type')=='error':raise RuntimeError(msg)
        if msg.get('type')==expected:return msg

async def ingest(endpoint,root,text,source,base,out):
    if len(source.encode())>256:source='source-sha256:'+hashlib.sha256(source.encode()).hexdigest()
    parts=chunks(text)
    if not parts:raise ValueError('no usable source text')
    run=uuid.uuid4().hex;record={'run':run,'source':source,'text_sha256':hashlib.sha256(text.encode()).hexdigest(),'chunks':parts}
    out=pathlib.Path(out);out.parent.mkdir(parents=True,exist_ok=True)
    evidence=out.with_name(out.name+'.learning.json')
    if evidence.exists() or out.exists():raise ValueError('output exists; choose a new immutable output path')
    evidence.write_text(json.dumps(record,indent=2)+'\n')
    async with websockets.connect(endpoint,max_size=262144,ping_interval=None,origin=None) as ws:
        await exchange(ws,{'schema':1,'type':'root.select','request_id':run,'root_id':root,'purpose':'source learning'},'root.selected')
        for i,part in enumerate(parts):
            pid=f'{run}-{i}'
            packet={'schema':1,'intent':'memory.upsert','hops':0,'root_id':root,'engram_id':pid,'packet_id':pid,'expected_version':0,
                'expires_ms':int(time.time()*1000)+60000,'text':part,'source':source,'claim_status':'hypothetical'}
            await exchange(ws,{'schema':1,'type':'memory.upsert','request_id':pid,'packet':packet},'memory.accepted')
        report=await asyncio.to_thread(train,parts,base,out,source)
        record['training']=report;evidence.write_text(json.dumps(record,indent=2)+'\n')
        await exchange(ws,{'schema':1,'type':'learning.record','request_id':run+'-learn','root_id':root,'report':report},'learning.candidate')
    return report

async def main(args):
    if args.url:
        url,text,digest=await asyncio.to_thread(fetch,args.url);source=url+'#sha256='+digest
    else:
        p=pathlib.Path(args.text);text=p.read_text();source='file-sha256:'+hashlib.sha256(p.read_bytes()).hexdigest()
    print(json.dumps(await ingest(args.ws,args.root,text,source,args.base,args.output),indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();g=p.add_mutually_exclusive_group(required=True);g.add_argument('--url');g.add_argument('--text');p.add_argument('--ws',default='ws://127.0.0.1:8765');p.add_argument('--root',required=True);p.add_argument('--base',default='packages/breeze');p.add_argument('--output',required=True);asyncio.run(main(p.parse_args()))
