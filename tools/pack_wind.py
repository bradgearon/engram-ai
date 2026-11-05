#!/usr/bin/env python3
"""Portable lightweight JSON -> aligned little-endian WDS Wind v1."""
import argparse, json, math, pathlib, struct

def fnv(data):
    h=14695981039346656037
    for b in data: h=((h^b)*1099511628211)&0xffffffffffffffff
    return f'{h:016x}'

def pack(weights, output, version=1, evidence=None):
    w=weights['weight']; b=weights['bias']
    if len(w)!=256 or len(b)!=16 or not all(math.isfinite(x) and abs(x)<=4 for x in w+b):
        raise ValueError('expected bounded finite 16x16 matrix and 16 bias values')
    output=pathlib.Path(output)
    output.mkdir(parents=True,exist_ok=False) # immutable package directory
    header=struct.pack('<8sIIII',b'WDSWND1\0',1,16,64,272).ljust(64,b'\0')
    data=header+struct.pack('<272f',*(w+b))
    manifest={'schema':1,'id':'wds.ai.winds.breeze','policy_version':version,'state_schema':1,
      'training_version':version,'evaluator_version':'structural-rules/1','state_dim':16,
      'alignment':64,'dtype':'f32-le','shape':[16,16],'checksum_fnv1a64':fnv(data),
      'maturity':'growing' if evidence else 'seeded','foundation_contract':'frozen decoder-only GGUF or deterministic demo',
      'update_rule':'z_next=clip(z+0.25*tanh(W*(z+pooled[:16])+bias+W[:,15]*feedback),-1,1)',
      'evidence':evidence or {},'promotion':'manual only; no mature claim from surrogate loss'}
    (output/'policy.wind').write_bytes(data)
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (output/'weights.json').write_text(json.dumps(weights,indent=2)+'\n')
    return manifest
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input');p.add_argument('--output',required=True);p.add_argument('--version',type=int,default=1);a=p.parse_args()
    w=json.loads(pathlib.Path(a.input).read_text()) if a.input else {'weight':[0.1 if i//16==i%16 else 0.0 for i in range(256)],'bias':[0.05]*16}
    print(json.dumps(pack(w,a.output,a.version),indent=2))
