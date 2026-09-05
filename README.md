# SPL Labs

A collection of systems programming labs in C and 32-bit x86 assembly for Linux.
The labs cover memory and pointers, command-line programs, processes and signals,
interprocess communication, binary files, assembly arithmetic, and the ELF
(Executable and Linkable Format) used by Linux executables and object files.

Each lab below links to its own README for assignment details, build commands,
usage examples, tests, and known limitations. Source paths mentioned in each
description are relative to that lab's outer folder.

- **[Lab 1 — Program Memory, Pointers, and Function Pointers](<Lab1-Program Memory and Pointers, Debugging and Simulating Object Oriented Programming/readme.md>)**
  Explores C memory layout, pointer arithmetic, arrays, dynamic allocation, and
  debugging with GDB. Includes a word-counter debugging exercise, a program that
  prints memory addresses, and a character-processing menu built with structures
  and function pointers to demonstrate object-like behavior. The working
  exercises are in `task3/`; the lab README documents remaining code issues.

- **[Lab 2 — Linux Shell, Processes, Signals, and Redirection](<Lab2-Building a Linux Shell — Processes, Signals, Redirection, and Pipes/readme.md>)**
  Builds a simple shell using command parsing, `fork`, `execvp`, and process
  waiting. Supports foreground and background commands, changing directories,
  input/output redirection, and signal commands. `Lab2/myshell.c` contains the
  shell, `looper.c` demonstrates signal handling, and `mypipe.c` demonstrates
  parent-child communication through a pipe. Pipelines are a separate exercise;
  this shell does not execute piped commands.

- **[Lab 3 — Assembly Programming and Linux System Calls](<Lab3-Assembly Programming and Linux System Calls/readme.md>)**
  Introduces 32-bit x86 assembly, startup code, C/assembly calling conventions,
  and direct Linux system calls without libc. `Lab3/task1/` implements a stream
  encoder with argument reporting and file input/output. `Lab3/task2/` combines
  C and assembly to append a defined assembly-code region to a selected file as
  an educational file-infection exercise.

- **[Lab 4 — ELF Binary Analysis, Hex Editing, and Patching](<Lab4-ELF Binary Analysis, Hex Editing, and Patching/readme.md>)**
  Examines executable headers, symbols, file offsets, and virtual addresses.
  `Lab4/task1.c` implements `hexeditplus`, a menu-driven tool for loading,
  displaying, modifying, and saving binary data in different unit sizes.
  Includes supplied binaries for entry-point repair and function-patching
  exercises, plus a replacement digit-counting function in `Lab4/task4.c`.

- **[Lab 5 — Static ELF32 Loader](<Lab5-Building a Static ELF32 Loader in C/readme.md>)**
  Builds an educational loader that inspects ELF32 program headers, maps loadable
  segments into memory, initializes the zero-filled data region, applies memory
  permissions, and transfers control to the executable's entry point while
  forwarding arguments. `Lab5/my_loader.c` works with assembly startup routines
  and a custom linker script. Execution supports static i386 executables within
  the loader's documented limits.

- **[Lab A — Command-Line Stream Encoder in C](<LabA-Command-Line Stream Encoder in C/Readme.md>)**
  Practices command-line argument parsing, standard streams, file I/O, and debug
  output. `LabA/encoder.c` encodes or decodes lowercase letters and digits using
  a repeating numeric key, with wraparound in each character range. It supports
  input/output files and passes input through unchanged when no key is supplied.
  The folder also includes introductory C/assembly helper examples.

- **[Lab B — Virus Detection and Neutralization with Linked Lists](<LabB-Virus Detection and Neutralization in C Using Linked Lists/readme.md>)**
  Uses structures, linked lists, dynamic memory, and binary file I/O to load
  signature databases and find matching byte sequences. `LabB/AntiVirus.c`
  reports matches in the first 10 KiB of a file and can replace each match's
  first byte with an x86 return instruction as a lab neutralization exercise.
  Includes sample signatures, an example infected file, bubble sort, and a
  hexadecimal-printing utility.

- **[Lab C — Shell Pipelines, Process Control, and Command History](<LabC-Building a Linux Shell with Pipes, Process Control, and Command History/readme.md>)**
  Extends the shell topics with two-command pipelines, redirection, background
  execution, and a process list that tracks running, suspended, and terminated
  children. `LabC/myshell.c` provides signal-based process controls and a
  20-entry command history with recall. `mypipeline.c` demonstrates
  `ls -l | tail -n 2`, and `looper.c` supports signal experiments.

- **[Lab D — Multi-Precision Arithmetic in x86 Assembly](<LabD-Multi Precision Arithmetic in x86 Assembly./README.md>)**
  Implements addition of unsigned integers larger than a CPU register using
  byte arrays and carry propagation. `multi.s` includes hexadecimal input and
  output, memory allocation, C library calls, and a pseudo-random number
  generator. Operands can be predefined, entered by the user, or generated;
  an alternate introductory program demonstrates accessing `argc` and `argv`.

- **[Lab E — ELF32 Inspection and Linker Merge Validation](<LabE-ELF32 File Inspection and Linker Merge Validation/readme.md>)**
  Uses memory-mapped files to inspect ELF32 headers, sections, and symbol tables.
  `LabE/myELF.c` can keep two files open and check their symbols for undefined
  names and duplicate definitions before a potential merge. Includes example
  object files for these checks. The optional operation that actually merges
  ELF files remains unimplemented.

The lab folders contain their original task descriptions, source code, build
files, and supporting examples; several also include automated tests. Some keep
starter files beside a nested implementation directory, so follow the individual
README to choose the correct sources and build location.

Most C exercises use GCC and GNU Make on Linux. The assembly labs also use NASM
and GNU binutils. Several labs require 32-bit compilation or execution support;
those that link against libc may also require 32-bit development libraries.
Python 3 is used by the included automated test suites. Build and test commands
are specific to each lab; there is no shared build command at this root.
