"""Integration checks; all attachment targets live in a temporary directory."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ENC = ROOT / 'Lab3/task1/run_program'
ATT = ROOT / 'Lab3/task2/run_program'


def run(program, *args, data=b''):
    return subprocess.run([str(program), *map(str, args)], input=data,
                          capture_output=True, timeout=5)


source = bytes(range(256))
expected = bytes(c + 1 if 65 <= c <= 122 else c for c in source)
p = run(ENC, data=source)
assert (p.returncode, p.stdout, p.stderr) == (0, expected, str(ENC).encode() + b'\n')
assert run(ENC).returncode == 0
with tempfile.TemporaryDirectory(prefix='lab3-test-') as directory:
    d = Path(directory)
    src, dst = d / 'input with spaces', d / 'output'
    src.write_bytes(source)
    for options in [(f'-i{src}', f'-o{dst}'), (f'-o{dst}', f'-i{src}')]:
        dst.write_bytes(b'old contents' * 100)
        p = run(ENC, *options)
        assert p.returncode == 0 and p.stdout == b''
        assert dst.read_bytes() == expected
        assert p.stderr.splitlines()[1:] == [s.encode() for s in options]
    assert run(ENC, f'-i{src}').stdout == expected
    assert run(ENC, f'-o{dst}', data=source).returncode == 0
    assert dst.read_bytes() == expected
    for args in [('-i',), ('-o',), ('-x',), (f'-i{d / "missing"}',),
                 (f'-o{d}',), (f'-i{d}',), ('-o/dev/full',)]:
        assert run(ENC, *args, data=b'A').returncode != 0, args

    # Compare appended bytes with the exact assembled code_start/code_end region.
    symbols = subprocess.check_output(['nm', '-n', str(ATT)], text=True)
    addresses = {line.split()[2]: int(line.split()[0], 16)
                 for line in symbols.splitlines() if len(line.split()) == 3}
    raw = d / 'text.bin'
    subprocess.run(['objcopy', '-O', 'binary', '--only-section=.text', str(ATT), str(raw)], check=True)
    start = addresses['code_start'] - addresses['_start']
    end = addresses['code_end'] - addresses['_start']
    payload = raw.read_bytes()[start:end]
    assert payload
    for name, original in [('A', b'original\x00data\xff'), ('second file', b'')]:
        target = d / name
        target.write_bytes(original)
        p = run(ATT, f'-a{target}')
        assert p.returncode == 0, p.stderr
        assert target.read_bytes() == original + payload
        assert p.stdout == b'Hello, Infected File\n' + str(target).encode() + b' VIRUS ATTACHED\n'
        assert not p.stderr
        assert run(ATT, f'-a{target}').returncode == 0
        assert target.read_bytes() == original + payload * 2
    for args in [(), ('-a',), ('file',), ('-a', 'file'), ('-xfile',),
                 (f'-a{d / "missing"}',), (f'-a{d}',), ('-a/dev/full',)]:
        p = run(ATT, *args)
        assert p.returncode == 0x55, (args, p.returncode)
        assert b'VIRUS ATTACHED' not in p.stdout
    assert not (d / 'missing').exists()
print('PASS: encoder byte range, streams, options, errors; exact attachment bytes on two files, repeated appends, exit codes.')
