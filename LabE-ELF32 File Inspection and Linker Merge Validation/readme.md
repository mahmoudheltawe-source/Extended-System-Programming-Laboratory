# Lab E: ELF32 inspection and merge checks

The program in `LabE/myELF.c` implements required parts 0, 1, 2 and 3.1
of `LabE-tasks`. Part 3.2 (the optional merger) remains unimplemented:
menu option 5 prints a message and does not create `out.ro`.

## Build and run

Use Linux with GCC, Make and the 32-bit C development libraries installed.
The default build uses the lab-required `-m32` flag:

```sh
cd LabE
make
./myELF
```

On Debian/Ubuntu, the relevant build packages are `build-essential`,
`gcc-multilib` and `libc6-dev-i386`. If the build reports missing headers
such as `sys/mman.h` with `-m32`, check that the 32-bit development libraries
are installed.

For local inspection on a machine without multilib, a native executable
can still parse ELF32 files using `Elf32_*` structures:

```sh
make clean
make ARCH_FLAGS=
./myELF
```

This fallback does not satisfy the assignment's 32-bit compilation requirement.
Run `make clean` before switching build architectures. `make clean` removes
only the generated inspector and its intermediate object.

## Menu and example

- **0 — Toggle Debug Mode:** show mapping sizes, section table indices,
  section-name offsets, and symbol table sizes, offsets and string-table links.
- **1 — Examine ELF File:** enter one filename per invocation. Up to two
  files stay open and mapped. Paths are relative to the working directory;
  paths containing spaces are supported. Prints ELF identification, encoding,
  entry point, section header details and program header details.
- **2 — Print Section Names:** list each file's section indices, names,
  addresses, offsets, sizes and symbolic/numeric types.
- **3 — Print Symbols:** list symbols in every static or dynamic symbol table,
  using its linked string table. Shows values, section indices, section names
  and symbol names; special sections are labeled UND, ABS or COMMON.
- **4 — Check Files for Merge:** requires two files with exactly one static
  symbol table each. Reports unresolved names and duplicate definitions,
  continuing after errors. Local and anonymous symbols do not participate
  in cross-file matching. A successful check prints no symbol errors.
- **5 — Merge ELF Files:** optional bonus stub.
- **6 — Quit:** unmap files, close descriptors and exit. End-of-input also
  cleans up and exits.

From inside `LabE/`, select `1`, enter `F1a.o`, select `1` again and enter
`F2a.o`. Options `2` and `3` inspect both files; option `4` should report no
symbol errors. Quit and restart to select another pair. With `F1b.o` and
`F2b.o`, option `4` reports `my_exit` undefined and `print_it` multiply defined.

## Fixes and verification

The previous README's claim that everything required worked was inaccurate.
The fixes correct symbol-name lookup, add the missing symbol section names,
print filenames and section type names, avoid false conflicts between local
symbols, and use the required menu function table. Input handling now supports
invalid menu entries, EOF and long lines. ELF validation checks header sizes,
file ranges, section/string tables and symbol references before accessing them.
ELF file contents are read through a single read-only `mmap` per file.

Supported inputs are ELF32 with native byte order (little endian on x86).
ELF64, opposite-endian files, extended section numbering and extended symbol
section indices are rejected. Unknown section types retain their numeric value.
The merge check is the lab's limited name-based check, not a full linker with
weak/common symbol resolution or relocation processing.

From the repository root, run the regression checks (Python 3 and binutils
`readelf` required):

```sh
make -C LabE
python3 tests/check.py
```

Verification here used `make -C LabE ARCH_FLAGS=` because this environment
lacks 32-bit development headers. The native build compiled without warnings.
Tests compare supplied objects and `a.out` symbol values/names/section indices
against `readelf`, check ELF header counts/offsets, both merge-check pairs,
menu errors, the two-file limit, EOF and malformed files. The default `-m32`
build could not be verified in this environment.
