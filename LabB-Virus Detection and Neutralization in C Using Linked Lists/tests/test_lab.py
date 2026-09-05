"""Regression checks using temporary suspected files; originals are never modified."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LAB = ROOT / 'LabB'
PROGRAM = Path(os.environ.get('ANTIVIRUS', LAB / 'AntiVirus')).resolve()


def run(target, commands, code=0):
    result = subprocess.run([str(PROGRAM), str(target)], input=commands,
                            text=True, capture_output=True, cwd=LAB, timeout=10)
    assert result.returncode == code, result.stderr
    assert 'runtime error:' not in result.stderr, result.stderr
    assert 'AddressSanitizer' not in result.stderr, result.stderr
    return result


def database(path, endian, records):
    path.write_bytes((b'VIRB' if endian == '>' else b'VIRL') + b''.join(
        struct.pack(endian + 'H', len(sig)) + name.ljust(16, b'\0') + sig
        for name, sig in records))


with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    target = temp / 'suspected'
    original = (LAB / 'infected').read_bytes()
    target.write_bytes(original)
    result = run(target, '0\nsignatures-B\n1\n3\n4\n3\n5\n')
    assert result.stdout.count('Virus detected!') == 1, result.stdout
    assert 'Starting byte location: 263' in result.stdout
    assert 'Virus name: Doom' in result.stdout
    assert 'Virus signature size: 19' in result.stdout
    expected = bytearray(original)
    expected[263] = 0xC3
    assert target.read_bytes() == expected
    assert (LAB / 'infected').read_bytes() == original
    target.write_bytes(original)
    assert 'Virus detected!' not in run(target, '1\n3\n5\n').stdout

    for endian in ['<', '>']:
        db = temp / 'signatures'
        database(db, endian, [(b'1234567890123456', b'ABA'), (b'long', b'x' * 300)])
        target.write_bytes(b'ABABA')
        commands = f'0\n{db}\n1\n1\n2\n3\n4\n3\n5\n'
        result = run(target, commands)
        assert result.stdout.count('Virus detected!') == 2
        assert 'Starting byte location: 0' in result.stdout
        assert 'Starting byte location: 2' in result.stdout
        assert target.read_bytes() == b'\xc3B\xc3BA'
        for data in [b'', b'A']:
            target.write_bytes(data)
            result = run(target, f'0\n{db}\n1\n3\n4\n5\n')
            assert 'Virus detected!' not in result.stdout
            assert target.read_bytes() == data

    # Only the first 10 KiB are scanned, including a match ending at the boundary.
    database(db, '<', [(b'boundary', b'XYZ')])
    target.write_bytes(b'.' * 10237 + b'XYZXYZ')
    result = run(target, f'0\n{db}\n1\n3\n4\n5\n')
    assert result.stdout.count('Virus detected!') == 1
    assert 'Starting byte location: 10237' in result.stdout
    assert target.read_bytes()[-6:] == b'\xc3YZXYZ'

    good = temp / 'good'
    database(good, '<', [(b'kept', b'ABC')])
    target.write_bytes(b'ABC')
    for malformed in [b'VIRL\x01', b'VIRL\x03\0' + b'x' * 16 + b'A',
                      b'VIRL\0\0' + b'x' * 16]:
        db.write_bytes(malformed)
        result = run(target, f'0\n{good}\n1\n0\n{db}\n1\n3\n5\n')
        assert result.stderr
        assert result.stdout.count('Signatures loaded successfully.') == 1
        assert 'Virus name: kept' in result.stdout
    for bad_magic in [b'', b'VIR', b'NOPE']:
        db.write_bytes(bad_magic)
        assert 'magic' in run(target, f'0\n{db}\n1\n', code=1).stderr
    run(target, '')
    run(target, f'0\n{good}\n1\n')  # EOF after allocating the list.
    assert run(target, 'junk\n99\n5\n').stdout.count('INVALID OPTION!') == 2
    assert run(temp / 'missing', '3\n4\n5\n').stderr

sort = subprocess.run([str(LAB / 'Bubblesort'), '3', '4', '2', '1'],
                      capture_output=True, text=True, check=True)
assert 'Sorted array: 1 2 3 4' in sort.stdout
hex_output = subprocess.check_output([str(LAB / 'hexaPrint'), str(LAB / 'exampleFile')], text=True)
assert hex_output.split() == [f'{byte:02X}' for byte in (LAB / 'exampleFile').read_bytes()]
print('All regression checks passed.')
