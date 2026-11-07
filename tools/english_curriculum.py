"""Author a reproducible synthetic dialogue corpus; no runtime language rules.
Families and wording are split before training. These are invented worlds, not user memories.
"""
import json,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]

def build(count=48):
    rows=[]
    for split,variant in [('train',0),('validation',1),('test',2)]:
        n=count if split=='train' else max(8,count//4)
        for i in range(n):
            a=['Mira','Lena','Nora'][variant]+str(i);b=['Jon','Tao','Eli'][variant]+str(i)
            place=['garden','studio','library'][variant];item=['book','key','map'][variant]
            project=['Lantern','Harbor','Orchard'][variant]+str(i)
            scenes=[
                ('reference',[(f'{a} has a cat named {b}.','I will keep those names together in this conversation.'),
                    (f'He enjoys sleeping in the {place}. Who enjoys that?',f'{b}, the cat, enjoys sleeping in the {place}.')]),
                ('transfer',[(f'{a} handed the {item} to {b}.',f'{b} received the {item} from {a}.'),
                    (['Who gave it away?','Identify the giver.','Which person handed it over?'][variant],f'{a} gave the {item} to {b}.')]),
                ('reverse',[(f'{b} handed the {item} to {a}.',f'{a} received the {item} from {b}.'),
                    (['Who gave it away?','Identify the giver.','Which person handed it over?'][variant],f'{b} gave the {item} to {a}.')]),
                ('negation',[(f'{a} does not own the {item}. {b} owns it.',f'The {item} belongs to {b}.'),
                    (f'Does {a} own it?',f'No. You said {a} does not own the {item}; {b} does.')]),
                ('correction',[(f'{a} lives near the {place}.',f'You said {a} lives near the {place}.'),
                    (f'Correction: {a} moved to {project}.',f'The updated location is {project}.'),
                    (f'Where does {a} live now?',f'According to your correction, {a} lives in {project}.')]),
                ('uncertainty',[(f'{a} might arrive at the {place} tomorrow.','That is a possibility, not a confirmed arrival.'),
                    (f'Is {a} definitely coming?',f'We do not know. You said {a} might arrive.')]),
                ('order',[(f'{a} entered the {place} before {b}.',f'{a} arrived first.'),
                    (['Who entered second?','Who followed the first arrival?','Name the later arrival.'][variant],f'{b} entered second.')]),
                ('condition',[(f'The {project} door opens only when a key is present. There is no key.','The stated condition is not satisfied.'),
                    (['Can the door open under that rule?','Does the supplied rule permit opening?','Is opening allowed by those conditions?'][variant],'No. The rule requires a key, and you said there is none.')]),
                ('condition_positive',[(f'The {project} door opens when a key is present. The key is present.','The stated condition is satisfied.'),
                    (['What does that rule predict?','Apply the rule to this situation.','What follows from those premises?'][variant],'The rule predicts that the door opens. That is a prediction, not an observation that it opened.')]),
                ('project',[(f'I am developing {project}, a metroidvania about a lost {item}.',f'{project} is your metroidvania project about a lost {item}.'),
                    (['What project am I working on?','Remind me what I am building.','Which game did I describe?'][variant],f'You are developing {project}, a metroidvania about a lost {item}.')]),
                ('preference',[(f'I like visiting the {place}, but {a} prefers {project}.',f'Your preference is the {place}; {a} prefers {project}.'),
                    (['What do I prefer?','Which place did I say I like?','Recall my preference, please.'][variant],f'You said you like visiting the {place}.')]),
                ('scope',[(f'In project {project}, the {item} is blue. In another project it is red.','Those are separate project descriptions.'),
                    (f'What color is it in {project}?',f'In {project}, the {item} is blue.')]),
                ('missing',[(f'{a} keeps a {item} at the {place}.','That tells us the object and location.'),
                    (f'How old is {a}?',f'The conversation does not state {a}\'s age. I should not infer it from the {item} or location.')]),
                ('comparison',[(f'{a} has {i+2} stones. {b} has {i+3} stones.',f'{b} has one more stone than {a}.'),
                    (['Who has fewer?','Which person has the smaller collection?','Name the person with less.'][variant],f'{a} has fewer stones.')]),
                ('hypothetical',[(f'Imagine {a} could fly to the {place}.',f'In that imagined situation, {a} could travel there by flying.'),
                    (f'Did we establish that {a} can really fly?','No. That was a hypothetical premise, not established knowledge.')]),
                ('clarify',[(f'{a} and {b} are at the {place}.','Both people are at that location.'),
                    ('They said it was ready.','Who said that, and what was ready? The reference is ambiguous.')]),
                ('polite',[(f'Hello, I am {a}.',f'Hello, {a}. What would you like to work on?'),
                    (f'Could we discuss {project}?',f'Yes. Tell me what you want to develop or understand about {project}.')]),
                ('story',[(f'Write a very short fictional story about {a} finding a {item}.',f'{a} found a {item} beside the {place}. It led to a small act of kindness rather than treasure.'),
                    ('Did that really happen?','No. That was a fictional story written for your request.')]),
                ('agency',[(f'I want to explore an idea for {project}, but I am unsure.',f'We can examine the idea and its tradeoffs. The decision remains yours.'),
                    ('Will you pretend to agree with everything?','I will respond respectfully and explain uncertainty or disagreement honestly.')]),
                ('recovery',[(f'We do not have a reliable source for the history of {project}.','We can record that gap and look for a suitable source.'),
                    ('Should we treat a guess as confirmed?','No. A guess should remain attributed and unverified until there is supporting evidence.')]),
            ]
            for kind,pairs in scenes:
                turns=[]
                for u,a_response in pairs:turns.extend([{'role':'user','text':u},{'role':'assistant','text':a_response}])
                rows.append({'id':f'{split}-{kind}-{i:04}','group':f'{kind}-wording-{variant}','split':split,
                    'source':'authored synthetic conversation; invented entities','turns':turns})
    return {'schema':'english.conversations/1','license':'project-authored synthetic examples','purpose':'next-symbol sequence training and held-out diagnostics; not Qwen-scale pretraining',
        'split_policy':'wording variants and entity sets assigned before optimization; whole conversations stay together','conversations':rows}

if __name__=='__main__':
    data=build();path=ROOT/'curricula/english-v2.json';path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'file':str(path),'conversations':len(data['conversations']),'turns':sum(len(r['turns']) for r in data['conversations'])}))
