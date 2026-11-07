"""Explicitly approve selected teacher demonstrations into a new training corpus.
No optimizer, inference, or automatic approval lives in this orchestration tool.
"""
import argparse,json,pathlib,hashlib
from curriculum_runner import atomic,load

def review(source,base,ids):
    wanted=set(ids);selected={r['source_instance']:r for r in source['lessons']}
    if not wanted or wanted-set(selected):raise ValueError('Approve at least one existing source_instance; unknown ids are rejected.')
    data=json.loads(json.dumps(base))
    known={r['id'] for r in data['conversations']}
    protected={t['text'].strip().casefold() for r in data['conversations'] if r['split']!='train' for t in r['turns'] if t['role']=='user'}
    for key in sorted(wanted):
        lesson=selected[key];identity='reviewed-'+hashlib.sha256((lesson['root_id']+'/'+key).encode()).hexdigest()[:24]
        if identity in known:raise ValueError('Lesson already imported: '+key)
        if lesson['input'].strip().casefold() in protected:raise ValueError('Reviewed input overlaps held-out curriculum; keep it out of training.')
        turns=lesson['context']+[{'role':'user','text':lesson['input']},{'role':'assistant','text':lesson['response']}]
        data['conversations'].append({'id':identity,'group':identity,'split':'train','turns':turns,
            'source':{'teacher_model':lesson['teacher_model'],'root_id':lesson['root_id'],'instance_id':key,'review_status':'operator_approved'},
            'claim_status':'approved language demonstration; does not establish factual truth'})
    return data
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lessons',required=True);p.add_argument('--base',default='curricula/english-v2.json');p.add_argument('--approve',nargs='+',required=True);p.add_argument('--output',required=True);a=p.parse_args()
    target=pathlib.Path(a.output);target.parent.mkdir(parents=True,exist_ok=True)
    atomic(target,review(load(pathlib.Path(a.lessons)),load(pathlib.Path(a.base)),a.approve));print('Saved reviewed training corpus: '+str(target))
