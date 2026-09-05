"""Regression checks; run after building LabE/myELF."""
import pathlib
import re
import struct
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
exe = root / 'LabE/myELF'
def run(commands):
    p = subprocess.run([str(exe)], input=commands, text=True, capture_output=True, cwd=root, timeout=5)
    assert p.returncode == 0, p.stderr
    return p.stdout + p.stderr

for obj in sorted((root / 'LabE').glob('*.o')) + [root / 'a.out']:
    out = run(f'1\n{obj}\n2\n3\n6\n')
    assert 'Invalid or unsupported' not in out, (obj, out)
    data = obj.read_bytes()
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    assert f'Start of section headers: {h[6]} ' in out
    assert f'Number of section headers: {h[12]}' in out
    # Compare every symbol value, section index and name with binutils.
    reference = subprocess.check_output(['readelf', '-sW', str(obj)], text=True)
    for line in reference.splitlines():
        m = re.match(r'\s*(\d+):\s+([0-9a-f]+)\s+\d+\s+\S+\s+\S+\s+\S+\s+(\S+)\s*(.*)', line)
        if not m:
            continue
        index, value, ndx, name = m.groups()
        name = name.split('@')[0]
        if ' SECTION ' in line:
            name = ''  # readelf synthesizes names for anonymous section symbols
        ndx = {'UND': '0', 'ABS': '65521', 'COM': '65522'}.get(ndx, ndx)
        assert re.search(r'\[\s*' + index + r'\] ' + value + ' ' + ndx + r' \S+ ' + re.escape(name) + r'(?:@@?[^ \n]+)?\n', out), (obj, line)

good = run('1\nLabE/F1a.o\n1\nLabE/F2a.o\n4\n6\n')
assert 'undefined' not in good and 'multiply defined' not in good
bad = run('1\nLabE/F1b.o\n1\nLabE/F2b.o\n4\n6\n')
assert bad.count('Symbol print_it multiply defined') == 1
assert 'Symbol my_exit undefined' in bad
assert 'Invalid option' in run('hello\n99\n')
assert 'No ELF files' in run('2\n3\n4\n')
assert 'Can only handle' in run('1\nLabE/F1a.o\n1\nLabE/F2a.o\n1\n6\n')
assert 'optional bonus' in run('5\n6\n')
with tempfile.TemporaryDirectory() as d:
    base = bytearray((root / 'LabE/F1a.o').read_bytes())
    cases = [b'', b'not ELF', bytes(base[:30])]
    for offset, fmt, value in [(4, 'B', 2), (5, 'B', 2), (32, 'I', 0xfffffff0), (50, 'H', 999)]:
        data = base.copy()
        struct.pack_into('<' + fmt, data, offset, value)
        cases.append(data)
    shoff = struct.unpack_from('<I', base, 32)[0]
    data = base.copy()
    struct.pack_into('<I', data, shoff + 40, 0xffffffff)
    cases.append(data)
    for i, data in enumerate(cases):
        path = pathlib.Path(d) / f'invalid file {i}'
        path.write_bytes(data)
        out = run(f'1\n{path}\n1\nLabE/F1a.o\n3\n6\n')
        assert 'Error:' in out and 'system_call' in out
print('All regression checks passed.')
