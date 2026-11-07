"""Preserve native failures through the supervisor and its event stream."""
import asyncio
import contextlib
import io
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from process_errors import CommandFailure, exit_description
from workshop import command
from curriculum_runner import event_sink


async def test():
    assert '0xC00000FD: stack overflow' in exit_description(3221225725, 'nt')
    assert '0xC00000FD: stack overflow' in exit_description(-1073741571, 'nt')
    assert '0xC0000005: access violation' in exit_description(3221225477, 'nt')
    assert 'SIGSEGV' in exit_description(-11, 'posix')
    with tempfile.TemporaryDirectory(prefix='wds-command-') as directory:
        root = Path(directory); script = root / 'child.py'; log = root / 'trainer.log'
        script.write_text('import sys\nprint(\'{"type":"language.stage","stage":"publishing candidate"}\', flush=True)\nprint("fixture filesystem failure", file=sys.stderr, flush=True)\nsys.exit(7)\n')
        events = []; token = event_sink.set(events.append)
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                try: await command(sys.executable, script, log_path=log)
                except CommandFailure as error:
                    assert error.returncode == 7 and 'fixture filesystem failure' in str(error)
                    assert str(log) in str(error) and 'exit code 7' in str(error)
                else: raise AssertionError('nonzero exit was reported as success')
        finally: event_sink.reset(token)
        assert any(e['type'] == 'language.stage' for e in events)
        assert any(e['type'] == 'workshop.command_failed' and e['returncode'] == 7 for e in events)
        text = log.read_text(); assert 'fixture filesystem failure' in text and 'PROCESS exit code 7' in text
        with contextlib.redirect_stdout(io.StringIO()):
            await command(sys.executable, '-c', 'print("resumed")', log_path=log)
        assert log.read_text().startswith(text) and 'resumed' in log.read_text(), 'retry overwrote failure log'
    print('Windows crash codes, native error text, phase events and append-only retry logs passed')


if __name__ == '__main__': asyncio.run(test())
