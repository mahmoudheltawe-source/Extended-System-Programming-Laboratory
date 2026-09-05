Lab 1: Program Memory, Pointers, and Function Pointers

This lab explores C memory layout, debugging with GDB, arrays and pointers,
dynamic memory allocation, and using structures with function pointers to
implement a menu with object-like behavior.

Files and tasks
---------------
- Lab1_Tasks: the assignment requirements.
- Task3-Examples: sample menu sessions supplied with the assignment.
- Root-level count-words.c, addresses.c, and base.c: starting/reference files;
  these include the original word-counter bug and unfinished exercises.
- task3/count-words.c (Task 0): a revised command-line word counter. The original
  single-word crash comes from trying to modify a string literal.
- task3/addresses.c (Task 1): prints addresses of local/global/static variables,
  functions, heap memory, arrays, and command-line arguments. It demonstrates
  address differences and pointer arithmetic: adding 1 advances a pointer by
  the size of its element type.
- task3/base.c and task3/menu_map.c (Tasks 2 and 3): currently identical programs.
  map allocates an array and applies a character function to each element.
  A structure array stores menu names and function pointers for reading,
  printing, encrypting, decrypting, and displaying character codes.
- task3/Makefile: builds base, count-words, and addresses with debug symbols.

Build and run
-------------
Use Linux with GCC and GNU Make. GDB is needed for the debugging exercise.
Run the following from the Lab1 directory:

    cd task3
    make

The Makefile does not build menu_map.c. Compile it separately:

    gcc -g -Wall menu_map.c -o menu

Run the programs from task3:

    ./count-words hello world
    ./addresses hello world
    ./menu

You can also run ./base for the same menu implementation.
The word counter treats each command-line argument as one word; quoting
"hello world" passes a single argument.

Task 1 specifically requests a 32-bit build. With 32-bit compiler/libc support
installed, build and run it using:

    gcc -m32 -g -Wall addresses.c -o addresses32
    ./addresses32 hello world

The existing Makefile uses the compiler's default architecture, without -m32.
Actual memory addresses vary between runs and systems.

Menu operations
---------------
Enter an option number followed by Enter:

    0  Get str              Read a line of text.
    1  Print str            Print each character on its own line; use '.' for
                            characters outside printable ASCII.
    2  Encrypt              Add 0x20 to characters in the range 0x20–0x4E.
    3  Decrypt              Subtract 0x20 from characters in the range 0x40–0x7E.
    4  Print Hex and Octal  Print each character's hexadecimal and octal values.

After selecting 0, enter the text on the next line. The implementation removes
its trailing newline. For a short demonstration, enter 0, then HEY!, then 1,
then 9, each on a separate line. It prints H, E, Y, and ! on separate lines;
9 is outside the menu bounds and exits. Ctrl+D at an empty option prompt also
exits. Repeated operations have a known memory issue described below.

Debugging
---------
To inspect the revised counter:

    gdb --args ./count-words hello

At the GDB prompt, use run to execute, backtrace to inspect the stack after a
fault, and quit to exit. To reproduce the original Task 0 bug, build the
root-level reference source into a separate executable from task3:

    gcc -g -Wall ../count-words.c -o count-words-original
    gdb --args ./count-words-original hello

Review findings
---------------
The previous statement that the lab "works 100%" is not supported by review.
Builds and runtime checks were performed on a temporary copy without changing
any source files or the Makefile.

- make succeeds. addresses runs, but GCC warns about an uninitialized pointer
  and a non-void function with no return statement.
- The revised counter avoids modifying a string literal, but starts its count
  at 1: zero, one, and two arguments report 1, 2, and 3 words respectively.
- The menu compiles; reading/printing a short string, out-of-range numeric
  selection, and EOF exit worked in the checks.
- AddressSanitizer confirms a heap-buffer-overflow when reading HEY!, printing
  it, then selecting Encrypt. map allocates only the character count without a
  terminating null byte, while later operations call strlen on the result.
  Reading another string after mapping can also exceed the smaller allocation,
  because the input call still assumes a 256-byte buffer.
- The menu differs from the requirements: it uses variable-length strings
  instead of a fixed five-character array, handles Get str separately instead
  of through map, and computes the menu bound inside the loop. my_get uses
  fgets instead of the Task 2 requirement to use fgetc. Non-numeric option
  input is interpreted as 0 by atoi.
- Task 1 does not print sizeof(long) or every cell address of the four arrays.
  Its demonstration of an uninitialized pointer has undefined behavior.
- The required -m32 build could not be verified here because 32-bit libc headers
  are unavailable. The normal build succeeded.
- The supplied menu examples and encryption prose are inconsistent. The code
  uses the explicit hexadecimal ranges listed above.

These findings describe the current code; they have not been fixed.

Cleanup
-------
From task3:

    make clean
    rm -f menu addresses32 count-words-original

make clean removes the three executables built by the Makefile. The second
command removes executables built manually using the instructions above.
