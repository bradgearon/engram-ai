"""Reproduce the bundled symbolic curricula. No optimizer or ML dependency."""
import hashlib,itertools,json,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]

def music(defects,variant,split,index):
    tonic=48+(variant%12); n=4+4*(variant%3); notes=[{'pitch':tonic+[0,2,4,0][i%4],'duration':2} for i in range(n)]
    req={'pitch_min':tonic,'pitch_max':tonic+24,'home_pc':tonic%12,'target_ticks':2*n,'return_home':2 in defects,'repeat_motif':5 in defects}
    if 4 in defects:
        req['divergence_pc']=(tonic+6)%12;notes[1]['pitch']=tonic+6
    if 5 in defects:
        for i in range(n//2,n):notes[i]=dict(notes[i-n//2])
    if 2 in defects:notes[-1]['pitch']=tonic
    if 3 in defects:
        req['answer_interval']=[3,4,7,8][variant%4];req['anchors']=[v['pitch']-req['answer_interval'] for v in notes]
    # Every input starts from a known constraint-compatible solution, then gains declared defects.
    if 3 in defects:
        for i,v in enumerate(notes):v['pitch']=req['anchors'][i]
    if 0 in defects:
        for v in notes:v['pitch']=max(0,v['pitch']-24)
    if 1 in defects:
        for v in notes:v['duration']=1
    if 2 in defects:notes[-1]['pitch']=tonic+2
    if 4 in defects:
        for v in notes:
            if v['pitch']%12==req['divergence_pc']:v['pitch']=tonic+1
    if 5 in defects:notes[-2]['pitch']=tonic+9
    return {'schema':1,'id':f'music-{split}-{index}','domain':'music','artifact':{'notes':notes},'requirements':req}

def architecture(defects,variant,split,index):
    nodes=[{'id':'domain','layer':'domain','writes':['domain-db'],'stateful':True,'supervised':True},
           {'id':'app','layer':'app','writes':['app-db'],'stateful':True,'supervised':True},
           {'id':'storage','layer':'adapter','writes':['storage-db'],'stateful':False,'supervised':True}]
    budget=[30,75,150,400][variant%4]
    edges=[{'from':'domain','to':'storage','kind':'call','via_port':True,'idempotent':True,'timeout_ms':budget},
           {'from':'app','to':'storage','kind':'event','via_port':True,'idempotent':True,'timeout_ms':0}]
    req={k:True for k in ['domain_isolation','private_ownership','event_idempotency','bounded_calls','supervision']}
    req.update(async_boundary=4 in defects,latency_budget_ms=budget)
    if 0 in defects:edges[0]['via_port']=False
    if 1 in defects:nodes[1]['writes']=['domain-db']
    if 2 in defects:edges[1]['idempotent']=False
    if 3 in defects:edges[0]['timeout_ms']=0
    if 5 in defects:nodes[variant%2]['supervised']=False
    if variant%2:
        nodes.append({'id':'worker','layer':'app','writes':['worker-db'],'stateful':True,'supervised':5 not in defects})
    return {'schema':1,'id':f'architecture-{split}-{index}','domain':'architecture','artifact':{'components':nodes,'edges':edges},'requirements':req}

def main():
    training=[(i,) for i in range(6)]+[(0,1),(1,2),(2,3),(3,4),(4,5),(0,5)]
    validation=[(0,2),(1,3),(2,4),(3,5)]
    test=[(0,3),(0,4),(1,4),(1,5),(2,5)]
    for domain,build in [('music',music),('architecture',architecture)]:
        folder=ROOT/'curricula'/domain
        splits={}
        for split,families,variants in [('train',training,range(4)),('validation',validation,range(4,7)),('test',test,range(7,11))]:
            tasks=[]
            for family in families:
                for variant in variants:
                    tasks.append({'family':'repair-'+ '-'.join(map(str,family)),'lesson_sources':['sources/extracted/'],'task':build(family,variant,split,len(tasks))})
            path=folder/(split+'.json');path.write_text(json.dumps(tasks,indent=2)+'\n')
            splits[split]={'path':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'tasks':len(tasks),'families':len(families)}
        manifest={'schema':1,'id':'wds.ai.curriculum.'+domain,'version':1,'domain':domain,
            'feature_schema':domain+'.deficits/1','action_schema':domain+'.repairs/1','evaluator':'wds.ai.heart.'+domain+'/1',
            'splits':splits,'max_steps':4,'training_epochs':40,'learning_rate':0.05,'max_trajectory_steps':4,
            'scope':'symbolic repair-action selection; no general expertise or subjective quality claim',
            'teacher_role':'optional source-linked critique only; never test labels or numerical acceptance',
            'activation':'separate explicit command after all domain episodes finish',
            'external_enrichment':'not fetched automatically; original source links retained',
            'sources':[{'path':str(p.relative_to(folder)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted((folder/'sources').rglob('*.md'))]}
        (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        (folder/'EVALUATION.md').write_text('''# Executable curriculum v1

These tasks are authored symbolic fixtures based on the supplied lesson themes. They measure repair-action selection under explicit constraints. They do not measure general domain understanding.

Training uses single defects and selected adjacent combinations. Validation and locked tests contain disjoint defect-combination families. The held-out test stays outside optimizer inputs. Variants change pitches, motif lengths, interval requests, component counts and budgets. This is limited compositional transfer within the fixture grammar, not an independent real-world benchmark.

The native runtime supplies observations, executable action outcomes and greedy improvement labels. A trajectory with no immediate improving action records Stop; local greedy labels may miss multi-step solutions. Music checks exact duration, pitch range, optional home return, positional pitch-class intervals, a requested divergent pitch and exact repeated motifs. They do not score emotion, AHD payoff or artistic merit. Architecture checks declared graph flags, ownership and delivery fixtures; it does not establish real latency, production resilience or a universally best architecture.

Candidate validation compares equal action budgets with the parent and a fixed-order refinement baseline. A separate input-only reference is always reported. If a real model is configured, a bounded foundation-only reference may be requested; it is labeled separately and is not substituted with the demo. No report or passing gate activates a package.
''')
if __name__=='__main__':main()
