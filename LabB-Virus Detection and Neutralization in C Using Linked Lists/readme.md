# Lab B — signature-based virus detector

The code implements Parts 1 and 2 of `LabB_Tasks` in `LabB/AntiVirus.c`.
It loads binary signatures into a linked list, detects matching byte sequences,
and repairs matches by replacing their first byte with the x86 near-return
instruction `C3`. The optional Part 0 programs are also included.

## Build and run

Requirements: a C11 compiler (GCC by default) and Make. Run from this directory:

```sh
cd LabB
make
cp infected infected-copy
./AntiVirus infected-copy
```

The command-line argument is the suspected file. Signature paths are relative
to the current working directory. The default signature file is `signatures-L`.

| Option | Action |
| --- | --- |
| 0 | Set the signature file path; an empty line keeps the current path. |
| 1 | Load signatures, replacing the previously loaded list on success. |
| 2 | Print names, lengths, and hexadecimal signatures. |
| 3 | Report every match with its zero-based byte offset, name, and length. |
| 4 | Scan and modify the suspected file in place, writing `C3` at each match's first byte. |
| 5 | Free the list and quit. End-of-input also frees the list and exits. |

Load signatures before detecting or fixing. Option 4 scans independently, so
option 3 is not required first. Printing an empty list or scanning without
matches produces no signature/match output; the menu still appears.

## Supplied infected-file example

**Use `signatures-B` for the supplied `infected` file.** It contains the matching
`Doom` signature (19 bytes) at offset **263**. The supplied `signatures-L` has no
matches in this file; these two databases are not merely different byte-order
encodings of identical signature data.

At the menu, enter the following lines:

```text
0
signatures-B
1
3
4
3
5
```

The first detection reports `Doom` at 263. Repair writes `C3` at that offset,
leaving the other bytes and file length unchanged. The second detection finds
no match. The original `infected` stays unchanged when using the copy above.
The bundled executable is a statically linked 32-bit x86 ELF; running it for the
manual Part 2a demonstration requires an environment supporting that format.
Verification here checked detection and exact repair bytes, not execution of
the bundled executable before and after repair.

## Implementation and fixes

- Both `VIRL` (little-endian) and `VIRB` (big-endian) signature lengths are decoded
  explicitly. Previously, big-endian input was accepted but decoded incorrectly.
- Detection and repair skip signatures longer than the available data, fixing
  unsigned subtraction underflow and out-of-bounds reads on small/empty files.
- As required by the lab, only the first **10 KiB (10,240 bytes)** are scanned.
  Matches extending past that boundary are not detected. All fully contained
  matches, including overlapping matches, are considered.
- Invalid/missing magic prints an error and exits unsuccessfully. Truncated
  records, zero-length signatures, and allocation failures are reported without
  accepting a partially loaded replacement list. Opening/parsing failures
  preserve the previous list, except fatal magic errors which end the program.
- Names print at most 16 bytes, list printing respects its output stream, and
  invalid menu input and EOF are handled. File writes and closure are checked
  before reporting successful repair.
- Repair uses the original scan snapshot so earlier writes do not hide
  overlapping matches. This is the lab's signature/RET simulation, not a general
  malware-removal program.

## Part 0 utilities

From `LabB/`:

```sh
make preparation
./Bubblesort 3 4 2 1
./hexaPrint exampleFile
```

Bubblesort prints the original numbers and `Sorted array: 1 2 3 4`, then frees
its array. Its missing-argument and allocation-error exit handling were fixed.
The added `hexaPrint` reads binary data with `fread` and uses `PrintHex` to print
space-separated uppercase hexadecimal bytes.

## Verification

From the repository's outer directory:

```sh
make -C LabB all preparation
python3 tests/test_lab.py
```

Regression checks pass for both byte orders, the supplied infected file, exact
repair bytes, overlapping matches, empty/short files, the 10 KiB boundary,
malformed databases, reloads, invalid input, EOF cleanup, and the Part 0 output.
All three programs compile without warnings using `-Wall -Wextra -Wpedantic`.
The detector's regression suite also passed AddressSanitizer and
UndefinedBehaviorSanitizer with leak detection disabled.

**The lab's Valgrind requirement remains to be verified locally:** Valgrind was
not installed in the review environment, and LeakSanitizer could not run under
its tracing restrictions. With Valgrind installed, run from `LabB/`:

```sh
valgrind --leak-check=full --show-leak-kinds=all ./AntiVirus infected-copy
valgrind --leak-check=full ./Bubblesort 3 4 2 1
```

Exercise loading/reloading, printing, detecting, fixing, and quitting in the
first command. `make clean` removes built programs and object files.

## Submission

The assignment requires only `AntiVirus.c` and lowercase `makefile` in the ZIP.
The default Make target builds only the detector, so those two files suffice.
From the outer directory, replacing `YOUR_ID` with your student ID:

```sh
mkdir -p submission
cp LabB/AntiVirus.c submission/AntiVirus.c
cp LabB/Makefile submission/makefile
(cd submission && zip ../YOUR_ID.zip AntiVirus.c makefile)
```

Extract the ZIP into an empty directory and run `make` to verify the submission.
Signature databases and suspected files are runtime inputs and are not included
in the required two-file archive.
