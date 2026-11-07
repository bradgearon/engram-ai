import json, pathlib, subprocess, sys, tempfile

trainer, verifier, parent = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix='wds-language-') as temp:
    root=pathlib.Path(temp);candidate=root/'candidate'
    def rows(split,count):
        result=[]
        for target in range(3):
            for n in range(count):
                features=[0.]*16;features[target]=1.;features[3+n%13]=.02*(n+1)
                result.append({'features':features,'target':target,'text':f'{split}-{target}-{n}',
                               'representation_id':'synthetic-language-fixture/1'})
        return result
    job={'domain':'language','feature_schema':'language.projection/1','representation_id':'synthetic-language-fixture/1',
         'parent':parent,'output':str(candidate),'epochs':160,'learning_rate':.05,'backend':'scalar-sgd',
         'train':rows('train',8),'validation':rows('validation',3)}
    (root/'job.json').write_text(json.dumps(job))
    train=subprocess.run([trainer,'--job',str(root/'job.json')],capture_output=True,text=True,timeout=120)
    assert train.returncode==0,train.stderr
    assert (candidate/'policy.wind').stat().st_size==1152
    manifest=json.loads((candidate/'manifest.json').read_text());assert manifest['maturity']=='seeded'
    test={'package':str(candidate),'rows':rows('test',4),'minimum_strength':.75,'minimum_margin':.25}
    (root/'test.json').write_text(json.dumps(test))
    report=json.loads(subprocess.check_output([trainer,'--evaluate',str(root/'test.json')],text=True))
    assert report['accuracy']==1 and report['coverage']==1 and report['activated'] is False,report
    subprocess.run([verifier,str(candidate)],check=True,timeout=20)
    # Evaluation changes no bytes and training cannot silently reuse a mixed model.
    original=(candidate/'policy.wind').read_bytes()
    job['train'][0]['representation_id']='other-model';(root/'invalid.json').write_text(json.dumps(job))
    bad=subprocess.run([trainer,'--job',str(root/'invalid.json')],capture_output=True,text=True,timeout=20)
    assert bad.returncode and 'mixed model' in bad.stderr
    assert (candidate/'policy.wind').read_bytes()==original
    print('Native Language Wind training passed on synthetic features; this is not a Qwen language-quality score.')
