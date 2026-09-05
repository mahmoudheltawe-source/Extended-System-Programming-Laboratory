Lab 4 — ELF introduction and binary editing

Status
------
Task 1 is implemented in Lab4/task1.c. Task 4's count_digits function in
Lab4/task4.c correctly counts ASCII digits '0' through '9'. Tasks 0, 2 and 3
are ELF inspection/editing exercises; Task 4 also requires patching ntsc.
The supplied auxiliary binaries have not been modified.

Fixed problems in the original code:
- Display/save now interpret nonzero addresses as virtual memory addresses.
- Load limits depend on the byte count, not the input file offset.
- Negative/oversized lengths and out-of-range buffer modifications are rejected.
- Save checks the file size before writing and does not truncate the file.
- File seeking, short reads and write/close failures are checked/reported.
- Menu input handles invalid text and EOF; menus use the function-pointer array.
- Decimal display uses signed 1-, 2- or 4-byte values, as in the lab examples.
- The build defaults to 32-bit and disables PIE and stack protection.
- Submission filenames now match the requirements.

Build and run
-------------
Use Linux with GCC, make, binutils and 32-bit C development/runtime support.
On Debian/Ubuntu the relevant packages can be installed with:
    sudo apt install build-essential gcc-multilib libc6-dev-i386 binutils zip

From the directory containing this readme:
    cd Lab4
    make clean
    make
    ./hexeditplus
    ./Task4 'aabbaba123baacca'
    ./Task4 '0123456789'
The counter should report 3 and 10 digits, respectively. Quote empty strings
or strings containing spaces. With no argument it prints usage and exits 1.

The original filenames hexeditplus.c, Task4.c and Makefile are compatibility
wrappers. Edit task1.c, task4.c and makefile as the authoritative sources.

Validation on the current machine
---------------------------------
The machine lacks the 32-bit development libraries and /lib/ld-linux.so.2.
The required 32-bit build and execution of the supplied ELF32 programs could
therefore not be verified here. Native builds compiled without warnings and
passed tests for loading at large file offsets, short reads, signed display,
modify/save byte accuracy, real virtual-address saving, invalid input, bounds,
EOF and digit counting.

To reproduce native-only tests when 32-bit libraries are unavailable:
    cd Lab4
    make clean
    make ARCH_FLAGS=
    cd ..
    python3 tests/check.py
This creates native executables for testing only. Rebuild with `make clean`
and `make` before doing the ELF32 exercises or extracting a Task 4 patch.
The clean step matters when switching architectures: make does not track flags.

How hexeditplus works
--------------------
The editor maintains a filename, unit size (initially 1), a 10000-byte buffer,
a count of initialized bytes, and debug/display flags. Operations return to
the menu; option 8 or EOF exits. Debug diagnostics go to stderr and include
the buffer's current virtual address.

0  Toggle debug mode.
1  Set filename (relative to the directory where the editor was started).
2  Set unit size: 1, 2 or 4 bytes.
3  Load: <file-offset in hex> <number of units in decimal>.
   Replaces buffer contents from offset zero. Reports actual units read and
   any short read. mem_count tracks bytes, so changing unit size is safe.
4  Toggle decimal/hexadecimal display (decimal initially).
5  Display: <virtual address in hex> <number of units in decimal>.
   Address 0 selects the buffer start. Nonzero addresses must be valid readable
   addresses in this running process; invalid external addresses may crash it.
6  Save: <source virtual address in hex> <target file offset in hex>
         <number of units in decimal>.
   Source 0 selects the buffer start. Target must not exceed the existing file
   size; a write beginning at/before EOF may extend it. The file must exist.
7  Modify: <buffer byte offset in hex> <value in hex>.
   Writes the low unit_size bytes. Extending the initialized region zero-fills
   the gap. This offset is NOT a virtual address.
8  Quit.

Display/save within the buffer may only access initialized bytes. All transfer
lengths are capped at 10000 bytes. On x86 values are little-endian: bytes 01 02
represent 0x0201 when displayed as a 2-byte unit.

Example: inspect an ELF32 entry point
------------------------------------
From Lab4, start ./hexeditplus and enter these lines:
    1
    abc
    2
    4
    3
    18 1
    4
    5
    0 1
    8
This loads one four-byte unit at file offset 0x18 and displays it in hex.
ELF32's e_entry field is at 0x18. For the supplied abc it is 0x080483b0.
`readelf -h abc` confirms this and reports 29 sections (Task 0a).
Repeat with hexeditplus after a 32-bit build to inspect your own entry point;
its value depends on your compiler/build. ELF64 has an eight-byte e_entry.

Task 2: repair deep_thought
--------------------------
Work on a copy:
    cp deep_thought deep_thought.fixed
    chmod u+x deep_thought.fixed
    readelf -h deep_thought
    readelf -sW deep_thought
The supplied entry point is 0x08048464. The runtime entry routine _start,
which initializes execution before main, is at 0x08048350.
In hexeditplus, set filename to deep_thought.fixed, unit size to 4, then:
    option 3: 18 1
    option 4: toggle to hex
    option 5: 0 1
    option 7: 0 8048350
    option 6: 0 18 1
Quit, verify with `readelf -h deep_thought.fixed`, then run the copy on a
machine with 32-bit runtime support.

Task 3: inspect and disable offensive's main
-------------------------------------------
    readelf -sW offensive
    readelf -SW offensive
main has address 0x0804841d, size 23 bytes, section index 13 (.text).
.text has address 0x08048320 and file offset 0x320. Therefore:
    main file offset = 0x320 + (0x0804841d - 0x08048320) = 0x41d
In the editor, set filename to offensive, unit size 1, load `41d 23`,
toggle to hexadecimal, then display `0 23`.

To disable it, copy offensive to offensive.fixed and select that filename.
With unit size 1, modify `0 c3` and save `0 41d 1`. This places RET at the
first instruction of main, before its stack frame is changed. Verify using
`objdump -d offensive.fixed`. Running it should produce no application output;
a bare RET does not guarantee a particular exit status.

Task 4: replace count_digits in ntsc
-----------------------------------
The original function counts only '1' through '8', excluding '0' and '9',
and includes unnecessary delay loops. Thus 0123456789 exposes the error.
The replacement scans once and increments for '0' <= character <= '9'.

After a normal 32-bit build, run the counter examples above and inspect:
    readelf -sW Task4
    readelf -SW Task4
    objdump -d Task4
    readelf -sW ntsc
    readelf -SW ntsc
Find the replacement count_digits symbol's address, size and section. Compute
its file offset as section_offset + symbol_address - section_address.
Use your actual build's numbers; compiler versions may change them.

The supplied ntsc count_digits is at virtual address 0x0804847d, file offset
0x47d, and occupies 93 bytes. The replacement must fit in those 93 bytes,
use the same 32-bit calling convention, and return normally. Inspect the
replacement disassembly: copied code must not depend on external calls,
absolute addresses of its original data, PIE thunks or stack-canary helpers.
Internal relative branches remain valid when the whole function is copied.
The provided simple counter and required compiler flags support this approach;
verify the size/disassembly for your compiler before copying.

    cp ntsc ntsc.fixed
    chmod u+x ntsc.fixed
In hexeditplus:
    option 1: Task4
    option 2: 1
    option 3: <replacement file offset in hex> <replacement size in decimal>
    option 1: ntsc.fixed
    option 6: 0 47d <replacement size in decimal>
This uses Load and Save as required. Bytes after the replacement's return can
remain unchanged. Then verify the patched disassembly and run:
    ./ntsc.fixed aabbaba123baacca
    ./ntsc.fixed 1112111
    ./ntsc.fixed 0123456789
Expected counts: 3, 7 and 10. These patched runtime results have not been
verified on this machine because its 32-bit runtime is missing.

Submission
----------
From Lab4 run:
    make submission
lab4-submission.zip contains only task1.c, task4.c and makefile. Extract it to
an empty directory and run make and the examples before uploading, as required
by the lab. Auxiliary binaries, compatibility wrappers and tests are excluded.
