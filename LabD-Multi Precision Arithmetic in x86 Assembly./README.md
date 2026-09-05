# Lab D: Multi-Precision Arithmetic in x86 Assembly

`multi.s` implements the complete assignment in 32-bit x86 NASM assembly.
It adds unsigned integers that can be much larger than a CPU register by
storing each number as an array of bytes. The final program prints the two
operands and their sum as lowercase hexadecimal, one value per line.

The files in this folder are:

| File | Purpose |
| --- | --- |
| [`multi.s`](multi.s) | Assembly implementation of Parts 0–4. |
| [`makefile`](makefile) | Builds the programs, runs tests, and creates the submission ZIP. |
| [`tests/test_multi.py`](tests/test_multi.py) | Checks command-line behavior and arithmetic against Python integers. |
| [`tests/test_functions.c`](tests/test_functions.c) | Calls the assembly functions directly to check their results and memory handling. |
| [`LabD-tasks.txt`](LabD-tasks.txt) | Original assignment requirements. |

Build on Linux with NASM, GCC, Make, and the 32-bit C development libraries.
On Ubuntu, install the dependencies with:

```sh
sudo apt-get update
sudo apt-get install nasm gcc-multilib libc6-dev-i386 make python3
```

Open a terminal in this `LabD` folder, then build the executable:

```sh
make clean
make
```

`make` assembles `multi.s` into `multi.o` and links it with the C standard
library to produce `multi`. The equivalent manual commands are:

```sh
nasm -f elf32 multi.s -o multi.o
gcc -m32 -no-pie multi.o -o multi
```

`-m32` selects a 32-bit executable. `-no-pie` is used because the assembly
refers to global data with absolute addresses. No C source is needed to build
the final program; Python and the C test harness are used for the extra tests.

Run without arguments to add the predefined `x_struct` and `y_struct` values:

```sh
./multi
```

Expected output:

```text
4f440201aa
4f44030201aa
4f9347040354
```

Run with `-I` to enter two hexadecimal numbers, pressing Enter after each
number. The program waits for both lines and then prints the operands and
the result. It does not display input prompts.

```sh
./multi -I
```

You can also supply the two lines through a pipe:

```sh
printf 'ffffffff\n1\n' | ./multi -I
```

Expected output:

```text
ffffffff
01
0100000000
```

Input accepts 1–599 hexadecimal digits per line, including odd lengths and
uppercase letters. LF, CRLF, and EOF after the last number are supported.
Enter digits only: do not include a `0x` prefix, a sign, or spaces.
The printer omits high zero bytes, keeps two digits per remaining byte, and
prints zero as `00`, so an input of `1` prints as `01`. The assignment permits
this leading zero.

Run with `-R` to generate two numbers and add them:

```sh
./multi -R
```

Each generated operand contains 1–255 bytes, so the output may be long.
The generator starts from the fixed nonzero seed `STATE = 0xace1`; restarting
the program therefore produces the same sequence. The flags are case-sensitive.
An unsupported flag, missing input, invalid hexadecimal input, or allocation
failure produces a message on stderr and exit status 1. Success returns 0.

The final `main` selects the operand source, calls `add_multi`, prints both
operands followed by the sum, and releases dynamically allocated memory.
The functions in `multi.s` divide that work as follows:

| Function | How it works |
| --- | --- |
| `print_multi(p)` | Traverses the byte array from its most significant byte to its least significant byte. Skips high zero bytes, calls `printf("%02hhx", byte)` for each remaining byte, and adds a newline with `puts`. |
| `getmulti()` | Reads a line using `fgets`. For an odd digit count, includes a reserved `0` before the input. Converts pairs of digits with `hex_digit` and stores the resulting bytes in reverse order in a newly allocated structure. |
| `hex_digit` | Internal helper that converts one ASCII hexadecimal character into a value from 0 to 15. Sets the carry flag for invalid characters. |
| `MaxMin` | Takes pointers in EAX and EBX. Compares their length fields and returns the longer operand in EAX and the shorter operand in EBX. Equal lengths retain their original order. |
| `add_multi(p, q)` | Uses `MaxMin` and allocates space for the header plus `max(p.size, q.size) + 1` bytes. Adds corresponding bytes with `ADC`, then propagates carry through the longer operand and stores the final carry in the extra byte. |
| `rand_num()` | Updates the 16-bit `STATE`. Masks bits with `MASK = 0x002d`, computes their XOR feedback using the parity flag, shifts the state right, and inserts the feedback at bit 15. Returns the new state in EAX. |
| `random_byte` | Internal helper that collects one output bit from each of eight calls to `rand_num`. |
| `PRmulti()` | Uses `random_byte` to choose a length, retrying if it is zero. Allocates a structure and fills its byte array with additional random bytes. |

The number layout corresponds to this C structure:

```c
struct multi {
    unsigned short size;  /* 16-bit number of bytes in num */
    unsigned char num[];  /* least significant byte first */
};
```

For example, `0x1234ab` has size 3 and bytes `ab 34 12`. The complete memory
representation is `03 00 ab 34 12`: the first two bytes hold the length and
the remaining bytes hold the number. Printing reverses the order of the
number's bytes; addition starts at the first byte so carries move toward
more significant bytes.

The handout contradicts itself about the structure layout. Its prose specifies
an `unsigned char` length, but its examples use `dw`, and 599 hexadecimal
digits need 300 bytes. This solution uses a **16-bit length followed by byte
elements** (`dw` for size, `db` for data). This also accommodates the required
extra carry byte after a 255-byte operand. Using `dw` for the individual data
elements, as printed in the handout, would insert zero bytes and contradict
the provided output. The interpretation is documented at the top of `multi.s`.

Except for `MaxMin`, the public functions follow CDECL: arguments are passed
on the stack, return values use EAX, and callees preserve EBX, ESI, EDI, and
EBP. The `ENTER` and `LEAVE` macros save and restore those registers and
provide stack alignment for calls into the C library. `add_multi` saves its
carry in DL between iterations because loop comparisons overwrite the flags.

`getmulti`, `add_multi`, and `PRmulti` return allocated structures or NULL on
failure. Each structure uses one allocation for its header and byte array.
`main` frees generated or entered operands and the sum; the predefined
operands are static data and are not freed. Addition leaves both operands
unchanged.

Part 0 is available through an alternate `main` in the same source:

```sh
make part0
./part0 first second
```

Expected output:

```text
3
./part0
first
second
```

This prints `argc` with `printf`, then prints every `argv` entry with `puts`,
including the program name. The makefile selects this alternate entry point
by assembling the same source with `-DPART0=1`.

Run the additional checks from this working folder:

```sh
make test
```

The Python tests compare actual program output with arbitrary-precision
integer addition for every input length from 1 through 599. The C test harness
checks the assembly functions directly, including operand preservation,
allocation guards and failures, all 65,535 nonzero LFSR states, and random
length retries. The harness assembles with `-DMULTI_LIBRARY=1` to omit the
assembly `main` and use its own C entry point. These test files are excluded
from the submission archive, so `make test` requires the full working folder.

To remove generated executables and object files, run `make clean`. This
preserves the source files, README, tests, and submission archive.

The development sandbox lacked 32-bit libraries and blocked 32-bit system
calls. Validation used Ubuntu libraries extracted under `/tmp/labd-toolchain`
and ran the executables outside that sandbox. No system packages were changed.
The currently built executables refer to those temporary libraries. To rebuild
on a machine with the packages above, run `make clean` followed by `make test`.
Linker errors mentioning missing `Scrt1.o`, `crti.o`, or `-lgcc` indicate that
the 32-bit development libraries are missing. If a copied executable reports
a missing interpreter or cannot find a `/tmp/labd-toolchain` path, rebuild it
with the installed libraries using the same clean-build commands.

Create the required submission archive with your student ID:

```sh
make submission ID=123456789
```

The archive contains exactly `multi.s` and `makefile`. A ready-made
`submission.zip` is also provided; rename it to your student ID before handing
it in.
