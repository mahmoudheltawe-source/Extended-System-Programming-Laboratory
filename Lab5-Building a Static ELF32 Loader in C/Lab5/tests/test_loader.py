"""End-to-end checks; needs Linux i386 execution support, gcc, nasm and ld."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LOADER = ROOT / 'my_loader'


def run(*args, **kwargs):
    return subprocess.run([str(x) for x in args], capture_output=True, timeout=10, **kwargs)


def load(path, *args):
    return run(LOADER, path, *args)


with tempfile.TemporaryDirectory(prefix='lab5-test-') as directory:
    d = Path(directory)
    asm = d / 'fixture.s'
    asm.write_text('''
section .text
global _start
_start:
    cmp dword [esp], 3
    jne fail
    mov eax, [esp+8]
    cmp dword [eax], 0x00656e6f ; "one"
    jne fail
    mov eax, [esp+12]
    cmp dword [eax], 0x006f7774 ; "two"
    jne fail
    cmp dword [esp+16], 0 ; argv terminator
    jne fail
    cmp dword [esp+20], 0 ; empty environment
    jne fail
    cmp dword [value], 0x12345678
    jne fail
    mov ecx, 16384
    mov esi, buffer
check:
    cmp byte [esi], 0
    jne fail
    inc esi
    loop check
    mov byte [buffer+16383], 42
    mov eax, 4
    mov ebx, 1
    mov ecx, message
    mov edx, 11
    int 0x80
    xor ebx, ebx
    jmp exit
fail:
    mov ebx, 42
exit:
    mov eax, 1
    int 0x80
section .rodata
message: db "fixture OK", 10
section .data
value: dd 0x12345678
section .bss
buffer: resb 16384
section .note.GNU-stack noalloc noexec nowrite progbits
''')
    subprocess.run(['nasm', '-f', 'elf32', str(asm), '-o', str(d / 'fixture.o')], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', str(d / 'fixture'), str(d / 'fixture.o')], check=True)
    result = load(d / 'fixture', 'one', 'two')
    assert result.returncode == 0 and b'fixture OK' in result.stdout, result
    print('PASS: file bytes, multi-page BSS, argument forwarding and stack terminators')

    result = run(LOADER, '--headers', d / 'fixture.o')
    assert result.returncode == 0 and result.stdout == b'', result
    result = run(LOADER, '--iterate', d / 'fixture')
    assert result.returncode == 0 and b'Program header number 0 at address' in result.stdout
    result = run(LOADER, '--headers', d / 'fixture')
    assert result.returncode == 0 and b'mmap protection=' in result.stdout
    print('PASS: header inspection and iterator, including an object with no program headers')

    original = (d / 'fixture').read_bytes()
    phoff = struct.unpack_from('<I', original, 28)[0]
    big = bytearray(original)
    big[5] = 2
    for offset, width in [(16, 2), (18, 2)] + [(i, 4) for i in range(20, 40, 4)] + [(i, 2) for i in range(40, 52, 2)]:
        big[offset:offset + width] = big[offset:offset + width][::-1]
    count = struct.unpack_from('<H', original, 44)[0]
    for offset in range(phoff, phoff + count * 32, 4):
        big[offset:offset + 4] = big[offset:offset + 4][::-1]
    (d / 'big-endian').write_bytes(big)
    assert run(LOADER, '--headers', d / 'big-endian').returncode == 0
    assert load(d / 'big-endian').returncode == 1
    print('PASS: big-endian ELF32 metadata inspection')
    variants = {'empty': b'', 'text': b'not ELF', 'truncated': original[:60]}
    for name, offset, fmt, value in [
        ('elf64', 4, 'B', 2), ('bad-table', 28, 'I', 0xfffffff0),
        ('dynamic', phoff, 'I', 2), ('interp', phoff, 'I', 3),
        ('bad-entry', 24, 'I', 0), ('bad-segment', phoff + 16, 'I', 0xffffffff),
        ('pie', 16, 'H', 3),
    ]:
        data = bytearray(original)
        struct.pack_into('<' + fmt, data, offset, value)
        variants[name] = data
    for name, data in variants.items():
        path = d / name
        path.write_bytes(data)
        result = load(path)
        assert result.returncode == 1 and result.stderr, (name, result)
    assert load(d / 'missing').returncode == 1
    assert run(LOADER).returncode == 1
    assert run(LOADER, '--headers', d / 'dynamic').returncode == 0
    print('PASS: malformed inputs and unsupported execution fail without jumping')

    # Link an executable over the loader itself: must refuse instead of clobbering it.
    subprocess.run(['ld', '-m', 'elf_i386', '-T', str(ROOT / 'linking_script'),
                    '-o', str(d / 'collision'), str(d / 'fixture.o')], check=True)
    result = load(d / 'collision', 'one', 'two')
    assert result.returncode == 1 and b'Cannot reserve' in result.stderr, result
    print('PASS: loader address collision is rejected')

    for name in ('loadme', 'encoder'):
        shutil.copyfile(ROOT / name, d / name)
        (d / name).chmod(0o700)
    direct = run(d / 'loadme')
    result = load(ROOT / 'loadme')
    assert result.returncode == direct.returncode and result.stdout.endswith(direct.stdout), result
    assert b'SHALOM' in result.stdout
    direct = run(d / 'encoder', input=b'Hello Lab5\n')
    result = run(LOADER, ROOT / 'encoder', input=b'Hello Lab5\n')
    assert result.returncode == direct.returncode and result.stdout.endswith(direct.stdout), result
    assert result.stderr == direct.stderr
    print('PASS: supplied loadme and encoder match direct execution')

headers = run('readelf', '-h', LOADER).stdout
segments = run('readelf', '-l', LOADER).stdout
assert b'ELF32' in headers and b'Intel 80386' in headers
assert b'INTERP' not in segments and b'DYNAMIC' not in segments
assert b'0x04048000' in segments
print('PASS: ELF32 static loader linked at the required alternative address')
