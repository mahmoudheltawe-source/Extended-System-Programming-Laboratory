Lab 3 — Assembly Language and Linux System Calls

Status
------
The mandatory Task 1 and Task 2 implementations in Lab3/task1 and
Lab3/task2 now build and pass the integration checks in tests/check.py.
They are 32-bit Linux ELF executables, linked directly with ld, without libc.
The root main.c/start.s/Util.* are starter material, not either task's build.
The root main.c still contains the optional Task 0 placeholder.

Requirements and build
----------------------
Use Linux on x86 with support for running 32-bit i386 executables. Install
NASM, GNU Make, GCC with support for -m32 compilation, and GNU binutils.
No 32-bit C runtime is needed because these programs do not link libc.
Python 3 is needed only for the integration checks.

From this directory (the directory containing Lab3-tasks):

    make -C Lab3/task1
    make -C Lab3/task2

To rebuild from scratch:

    make -C Lab3/task1 clean all
    make -C Lab3/task2 clean all

The executable in each task directory is named run_program. The make target
"run" also builds it; it does not launch the interactive program.

Task 1 — assembly encoder
-------------------------
All executable logic, including main, is in Lab3/task1/start.s.
_start passes argc and argv to main on the stack using cdecl.
The program prints every argument, including argv[0], on a separate line
to stderr. Lab3-tasks contradicts itself about stdout versus stderr for
Task 1.A; this implementation follows its always-on stderr debug description
so stdout contains only encoded data.

Input defaults to stdin and output defaults to stdout. Every byte in the
inclusive ASCII range 'A' through 'z' is increased by one. This includes
punctuation between 'Z' and 'a'; 'z' becomes '{'. Other bytes are unchanged.
Reads continue until EOF. Interrupted reads/writes are retried and short
writes are handled. Opened files are closed on successful completion;
on failure, process exit releases descriptors.

    printf 'Az!\n' | ./Lab3/task1/run_program

Encoded stdout is "B{!" followed by a newline; the executable name appears
separately on stderr.

    ./Lab3/task1/run_program -iinput.txt -ooutput.txt
    ./Lab3/task1/run_program -iinput.txt > output.txt 2> arguments.log

Options require the filename immediately after -i or -o. Either order is
accepted, with at most one of each. Quote an entire argument if its filename
contains spaces, for example '-imy input.txt'. Input must exist. Output is
created with mode 0644 (subject to umask), or truncated if it already exists.
Use distinct input and output files: output opening truncates its contents.
With interactive stdin, Ctrl-D signals EOF. Success returns 0; invalid
options and detected I/O errors print a diagnostic to stderr and return 1.

Task 2 — append the assembly code region
---------------------------------------
Lab3/task2/main.c validates exactly one nonempty -a{file} argument and passes
argv[1] + 2 as the filename. start.s supplies startup, the cdecl system_call
wrapper, infection(), and infector(filename). Assembly functions preserve
the caller's callee-saved registers.

infection() prints "Hello, Infected File" using one write syscall.
infector() prints the filename, opens the existing file with O_WRONLY |
O_APPEND, writes exactly code_end - code_start bytes from the contiguous
assembly text region, closes the file, and prints " VIRUS ATTACHED\n".
Messages used by these routines are included in the appended region.
Short writes and interrupted writes are handled. Success returns 0;
invalid arguments or detected I/O failures return 0x55 (85 decimal).
The success message is printed only after the append and close succeed.
A failed write can leave a partial append; there is no rollback.

Example using two disposable files:

    demo_dir=$(mktemp -d)
    printf 'first file\n' > "$demo_dir/A"
    printf 'second file\n' > "$demo_dir/B"
    ./Lab3/task2/run_program "-a$demo_dir/A"
    ./Lab3/task2/run_program "-a$demo_dir/B"
    od -Ax -tx1z "$demo_dir/A"

Expected output for each successful invocation:

    Hello, Infected File
    /path/to/file VIRUS ATTACHED

This modifies the selected file by appending bytes, preserving its existing
prefix. It does not create missing files. Each repeated invocation appends
another copy. Use disposable copies when experimenting.
Appending does not change an ELF entry point or make the appended code run
when the target is launched. The copied code contains addresses from this
program; it is not a standalone or relocatable executable.

What was fixed
--------------
* Task 1's Makefile no longer requires nonexistent main.c or unused C objects.
* Replaced broken argument printing/parsing and encoding I/O with assembly
  that reads cdecl arguments, uses real buffer addresses, and keeps input
  and output descriptors separate.
* Task 2 now handles the required combined -a{file} syntax once, strips the
  prefix, rejects malformed arguments, and propagates errors.
* Replaced the BSS placeholder for code_start with real text-region labels;
  corrected filename lengths, append flags, byte counts, and descriptors.
* Both builds use non-executable stack metadata; Task 2 C is compiled without
  PIE, stack protection, or standard-library dependencies.
* Corrected the string length in both optional HelloWorld.s examples to avoid
  writing an extra byte. They are not linked into the mandatory programs.

Verification
------------
After building, run:

    python3 tests/check.py

The checks passed on this machine: all 256 input byte values, empty input,
stdin/stdout and file options in both orders, output truncation, argument
printing, invalid options, missing files, directory I/O failures, /dev/full,
exact appended bytes on two temporary files, repeated appends, and exit codes.
The append checks compare against code_start/code_end in the built executable
using nm and objcopy, and verify the original file prefix is unchanged.
Both binaries are statically linked ELF32 files with no unresolved symbols.
The execution sandbox blocked i386 system calls with SIGSYS; runtime checks
passed when run outside that sandbox on the same host.

The original readme also mentioned an in-class question deduction. Runtime
checks cannot verify oral answers or predict a revised grade. For discussion:
Linux int 0x80 takes the syscall number in eax, arguments in ebx/ecx/edx,
and returns a result or negative error in eax. cdecl instead passes function
arguments on the stack. write needs a buffer address and an explicit byte
count, not a character value or a null-terminated-string assumption.
