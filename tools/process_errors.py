"""Native process failures with useful status codes, including Windows crashes."""
import os
import signal
import json


def exit_description(returncode, platform=None):
    platform = os.name if platform is None else platform
    if platform == 'nt' or returncode > 0x7fffffff:
        code = returncode & 0xffffffff
        names = {0xc00000fd: 'stack overflow', 0xc0000005: 'access violation',
                 0xc000001d: 'illegal instruction', 0xc0000135: 'missing dependent DLL',
                 0xc0000409: 'Windows fast-fail'}
        detail = names.get(code)
        return f'exit code {returncode} (0x{code:08X}' + (f': {detail}' if detail else '') + ')'
    if returncode < 0:
        try: name = signal.Signals(-returncode).name
        except ValueError: name = f'signal {-returncode}'
        return f'exit code {returncode} ({name})'
    return f'exit code {returncode}'


class CommandFailure(RuntimeError):
    def __init__(self, executable, returncode, tail='', log_path=None):
        self.returncode = returncode
        self.exit_status = exit_description(returncode)
        self.log_path = str(log_path) if log_path else ''
        self.output_tail = tail[-8192:]
        message = f'{executable} failed with {self.exit_status}.'
        details = []; phase = ''
        for line in self.output_tail.splitlines():
            try: event = json.loads(line)
            except ValueError: details.append(line); continue
            if isinstance(event, dict) and event.get('type') == 'language.stage': phase = event.get('stage', '')
            elif isinstance(event, dict) and event.get('type') == 'language.epoch': phase = f"completed epoch {event.get('epoch')}"
            elif not (isinstance(event, dict) and event.get('type', '').startswith('language.')): details.append(line)
        if phase: message += '\nLast stage: ' + phase
        if details: message += '\n' + '\n'.join(details[-3:])[-2048:]
        if self.log_path: message += '\nFull log: ' + self.log_path
        super().__init__(message)
