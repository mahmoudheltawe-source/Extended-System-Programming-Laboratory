"""Run after building Lab4/hexeditplus and Lab4/Task4."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
editor = root / 'Lab4/hexeditplus'

def run(commands):
    return subprocess.run([str(editor)], input=commands, text=True, capture_output=True, check=True, timeout=5)

with tempfile.TemporaryDirectory() as directory:
    source = pathlib.Path(directory) / 'source'
    target = pathlib.Path(directory) / 'target'
    source.write_bytes(bytes(range(256)) * 80)
    target.write_bytes(b'abcdefgh')
    result = run(f'1\n{source}\n3\n4000 4\n4\n5\n0 4\n7\n1 ff\n1\n{target}\n6\n0 2 4\n8\n')
    assert 'Loaded 4 units' in result.stdout
    assert '0\n1\n2\n3\n' in result.stdout
    assert target.read_bytes() == b'ab\x00\xff\x02\x03gh'
    result = run(f'1\n{target}\n3\n6 8\n6\n0 9 1\n3\n0 -1\n7\n2710 ff\n5\n0 3\ninvalid\n2\nx\n')
    assert 'Loaded 2 units' in result.stdout
    assert 'target location exceeds file size' in result.stdout
    assert 'invalid buffer location' in result.stdout
    assert 'range exceeds initialized buffer' in result.stderr
    assert target.read_bytes() == b'ab\x00\xff\x02\x03gh'
    for size, value, decimal in [(1, 'ff', '-1'), (2, '8000', '-32768'), (4, 'ffffffff', '-1')]:
        result = run(f'2\n{size}\n7\n0 {value}\n5\n0 1\n8\n')
        assert f'Decimal\n=======\n{decimal}\n' in result.stdout
    # Read a real virtual address reported by debug mode in this process.
    proc = subprocess.Popen([str(editor)], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    proc.stdin.write('0\n7\n0 41\n'); proc.stdin.flush()
    import re
    address = None
    while address is None:
        line = proc.stderr.readline()
        if 'mem_count=1 ' in line:
            address = re.search(r'mem_buf=(0x[0-9a-f]+)', line).group(1)
    proc.stdin.write(f'1\n{target}\n6\n{address} 0 1\n8\n'); proc.stdin.flush()
    proc.communicate(timeout=5)
    assert proc.returncode == 0 and target.read_bytes()[0] == 0x41

for text, expected in [('', 0), ('abc', 0), ('0123456789', 10), ('aabbaba123baacca', 3), ('1112111', 7), ('/0:9', 2)]:
    result = subprocess.run([str(root / 'Lab4/Task4'), text], capture_output=True, text=True, check=True)
    assert f'String contains {expected} digits' in result.stdout
print('All editor and digit-counter checks passed.')
