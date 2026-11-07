"""Low-stack training and recovery after the final epoch, without retraining."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def small_stack():
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_STACK, (256 * 1024, 256 * 1024))


def test(trainer, parent):
    with tempfile.TemporaryDirectory(prefix='wds-language-resume-') as directory:
        root = Path(directory)
        corpus = {'schema': 'english.conversations/1', 'conversations': []}
        for split in ('train', 'validation', 'test'):
            for i in range(3):
                corpus['conversations'].append({'id': f'{split}-{i}', 'group': split, 'split': split,
                    'turns': [{'role': 'user', 'text': f'{split} example {i}'}, {'role': 'assistant', 'text': 'The cat is here.'}]})
        data = root / 'corpus.json'; data.write_text(json.dumps(corpus))
        output = root / 'candidate'; checkpoint = root / 'candidate.checkpoint'
        job = root / 'job.json'
        job.write_text(json.dumps({'curriculum': str(data), 'parent': str(Path(parent).resolve()),
                                  'output': str(output), 'epochs': 2, 'learning_rate': .08}))

        def run(*args):
            options = {'preexec_fn': small_stack} if os.name == 'posix' else {}
            return subprocess.run([trainer, *map(str, args)], capture_output=True, text=True, timeout=45, **options)

        # An owned fixture blocks publication only after both epoch checkpoints.
        blocker = root / 'candidate.pending'; blocker.write_text('fixture blocks staging directory')
        failed = run('--language-job', job)
        assert failed.returncode == 1, (failed.returncode, failed.stderr)
        assert 'during packaging candidate' in failed.stderr, failed.stderr
        assert json.loads((checkpoint / 'state.json').read_text())['epoch'] == 2
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in checkpoint.iterdir()}
        blocker.unlink()
        resumed = run('--language-job', job)
        assert resumed.returncode == 0, (resumed.returncode, resumed.stderr)
        events = [json.loads(line) for line in resumed.stdout.splitlines()]
        assert any(e['type'] == 'language.resumed' and e['epoch'] == 2 for e in events)
        assert not any(e['type'] in ('language.epoch', 'language.batch') for e in events), 'completed epochs ran again'
        assert before == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in checkpoint.iterdir()}
        assert (output / 'policy.wind').stat().st_size == 1152
        assert (output / 'language.tensor').stat().st_size == 148800
        manifest = (output / 'manifest.json').read_bytes()
        repeated = run('--language-job', job)
        assert repeated.returncode == 0, repeated.stderr
        assert (output / 'manifest.json').read_bytes() == manifest
        evaluation = root / 'evaluate.json'; evaluation.write_text(json.dumps({'package': str(output), 'curriculum': str(data)}))
        evaluated = run('--language-evaluate', evaluation)
        assert evaluated.returncode == 0, (evaluated.returncode, evaluated.stderr)
        assert json.loads(evaluated.stdout)['activated'] is False
        tensor = output / 'language.tensor'; broken = bytearray(tensor.read_bytes()); broken[65] ^= 1; tensor.write_bytes(broken)
        corrupt = run('--language-job', job)
        assert corrupt.returncode == 1 and 'checksum mismatch' in corrupt.stderr, corrupt.stderr
    print('Low-stack native training, post-epoch recovery, unchanged checkpoints, immutable retry and corruption rejection passed')


if __name__ == '__main__': test(*sys.argv[1:])
