# Lab 5 — ELF32 static loader

The implementation is in `Lab5/`; the assignment is in `Lab5-tasks`.
`Lab5 auxiliary files/` contains the supplied reference files.

The code has been reviewed and corrected. It builds without libc and passes
`make test`, including execution of the supplied `loadme` and `encoder` programs.
This is a limited educational loader, not a replacement for the Linux ELF loader.

## Build

Use Linux on x86 with 32-bit i386 execution enabled. You need GNU Make, GCC with
`-m32` compilation support, the ELF development headers (`elf.h`), GNU binutils
(`ld` and `readelf`), and NASM. Python 3 is needed only for the tests.
No 32-bit libc runtime is linked or required by the resulting loader.

From this directory:

```sh
cd Lab5
make
```

The Makefile compiles freestanding 32-bit C, assembles `start.s` and `startup.s`,
and links them using `linking_script`. It disables PIE and compiler features
that would require runtime libraries. To rebuild from scratch, run
`make clean all`.

## Run

From `Lab5/`:

```sh
# Task 0: visit each program header and print its index and address.
./my_loader --iterate loadme

# Tasks 1a–1b: inspect headers and show mapping/protection flags.
./my_loader --headers loadme
readelf -l loadme

# Tasks 2a–2c: load and execute the supplied example.
./my_loader loadme

# Task 2d: forward arguments to a static executable.
./my_loader ./my_test_program arg1 arg2

# Run the supplied encoder with standard input.
printf 'Hello Lab5\n' | ./my_loader encoder
```

Replace `my_test_program` with your own compiled static i386 program. The loaded
program sees its filename as `argv[0]`, followed by the arguments you supplied.
The loader prints segment diagnostics to standard output before program output;
errors go to standard error. `loadme` prints
`SHALOM RAV SHOVECH TZIPORA NECHMEDET` and exits with status **1**, also when run
directly. This status is behavior of the supplied example, not a loading failure.

The input executable must be readable; it does not need its execute permission
set when passed to this loader. Inspection options do not execute the input.

## How the code works

1. `start.s` provides the loader's `_start` and `system_call` wrapper, using Linux
   i386 `int 0x80` system calls instead of libc.
2. `my_loader.c` opens and privately maps the input, checks the ELF header and
   program-header table bounds, and uses `foreach_phdr` to invoke callbacks.
   Inspection supports both ELF32 byte orders and accepts an empty header table.
3. Header output contains type (including its numeric value), file offset,
   virtual address, physical address, file size, memory size, R/W/E flags, and
   alignment. LOAD entries also show the translated `PROT_*` bits and mmap flags.
4. Execution requires a little-endian i386 `ET_EXEC` file without `PT_INTERP` or
   `PT_DYNAMIC`. The loader validates LOAD segment bounds and alignment and
   checks that the entry point belongs to an executable segment.
5. `load_phdr` handles nonempty `PT_LOAD` segments. It reserves page-aligned
   addresses with `MAP_FIXED_NOREPLACE` to avoid overwriting existing mappings,
   maps file bytes with `MAP_PRIVATE | MAP_FIXED`, zeroes the BSS region
   (`p_memsz - p_filesz`), and applies the segment's final R/W/X permissions.
   Empty segments are skipped. Any loading error prevents the entry-point jump.
6. `startup.s` constructs an aligned initial stack containing `argc`, `argv`,
   the terminating NULL, an empty environment, and an `AT_NULL` auxiliary-vector
   terminator. It jumps to the ELF entry point. The program then exits through
   its own system calls; the input file remains open and mapped during execution.

## Why `linking_script` matters

Ordinary lab executables are commonly linked near `0x08048000`. The supplied
script places this loader near **`0x04048000`**, keeping its code and data away
from the addresses where it loads those executables. Without that separation,
fixed-address mappings could overwrite the loader itself.

Verify the result with:

```sh
readelf -h my_loader
readelf -l my_loader
```

The header should identify ELF32, Intel 80386, and type EXEC. The entry point
should be in the loader's `0x04048...` area (the exact value changes with code
size). Program headers should show the alternative load address and no INTERP
or DYNAMIC segment. The runtime reservation check also rejects address clashes.

## Scope and limitations

- Execution targets static syscall-only programs from the earlier labs. Dynamic
  executables, PIE/shared objects, ELF64, non-i386 code, and runtimes requiring
  TLS setup or a complete Linux auxiliary vector are unsupported.
- The loaded program receives an empty environment. It must terminate itself;
  returning from `_start` is unsupported.
- LOAD segments must occupy separate page ranges. Segments sharing a page, or
  colliding with the loader, stack, or input mapping, are rejected.
- Execution requires kernel support for `MAP_FIXED_NOREPLACE` (Linux 4.17+).
  Files must fit in a positive signed 32-bit file size.
- Sandboxes that block i386 system calls may terminate the loader with
  `Bad system call`; run it in a Linux environment that permits these calls.

## Verification

```sh
make test
```

The suite builds a temporary syscall-only fixture and checks argument forwarding,
stack terminators, initialized data, writable multi-page zeroed BSS, header
iteration, empty program-header tables, big-endian metadata inspection, malformed
input rejection, unsupported executable rejection, and loader-address collision
handling. It compares `loadme` and `encoder` with direct execution and verifies
that the rebuilt loader is static ELF32 at the script's alternative address.
Temporary test executables are removed automatically.

Corrections from the original implementation include removing libc linkage,
restoring inspection/protection output, validating input before execution,
properly initializing BSS, stopping after mapping errors, completing the startup
stack, and fixing Makefile dependencies.
