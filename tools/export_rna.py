#!/usr/bin/env python3
"""Export replayed memory keys to a GPU-uploadable 256-byte record array.
JSONL remains authoritative; this is a disposable derived cache, not model weights.
"""
import argparse,json,pathlib,struct
from pack_wind import fnv
p=argparse.ArgumentParser();p.add_argument('chain');p.add_argument('output');a=p.parse_args()
memories={};prev='0000000000000000';seq=0
for line in pathlib.Path(a.chain).read_text().splitlines():
    e=json.loads(line);digest=e.pop('hash')
    # Preserve the C++ float spelling while excluding only the top-level hash member.
    decoder=json.JSONDecoder();pos=1;fields=[]
    while pos<len(line)-1:
        start=pos;key,end=decoder.raw_decode(line,pos);pos=end+1
        _,end=decoder.raw_decode(line,pos)
        if key!='hash':fields.append(line[start:end])
        pos=end+1
    canonical=('{'+','.join(fields)+'}').encode()
    if fnv(canonical)!=digest or e['seq']!=seq+1 or e['prev']!=prev:raise ValueError('broken chain integrity')
    seq=e['seq'];prev=digest
    if e['type']=='memory.accepted':memories[e['data']['id']]=e['data']
    for memory in e.get('data',{}).get('maintenance',{}).get('memories',[]):memories[memory['id']]=memory
rows=[v for k,v in sorted(memories.items()) if v['claim_status']!='retired']
header=struct.pack('<8sIIII',b'WDSRNA1\0',1,len(rows),256,64).ljust(64,b'\0')
data=header+b''.join(struct.pack('<64f',*(v['key']+[v['reliability']]+[0.0]*31)) for v in rows)
out=pathlib.Path(a.output);out.write_bytes(data)
out.with_suffix('.index.json').write_text(json.dumps({'schema':1,'source_chain_seq':seq,'source_chain_hash':prev,'checksum_fnv1a64':fnv(data),'rows':[{'id':v['id'],'version':v['version'],'source':v['source']} for v in rows]},indent=2)+'\n')
print(f'exported {len(rows)} aligned memory tiles')
