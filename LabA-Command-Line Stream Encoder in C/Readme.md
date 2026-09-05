# Lab A — Stream Encoder

The assignment encoder is implemented in `LabA/encoder.c`. It has been corrected
against `LabA_Tasks` and passes the regression suite. The previous README's
“code working 100%” statement was inaccurate.

## Build and run

Requirements: Linux, GCC (or another C99 compiler), and GNU Make. Python 3 is
needed only for the tests.

From the repository root:

```sh
make encoder
printf 'abcdez\n12#<\n' | ./LabA/encoder +e12345
```

The encoded stdout is:

```text
bdfhja
46#<
```

Debug messages appear separately on stderr. To save only the encoded output,
redirect stdout. To hide the debug trace entirely, redirect stderr to `/dev/null`.

You can also build and run directly in the source directory:

```sh
cd LabA
make encoder
./encoder +e12345
```

Type input and press Enter to process a line. Press Ctrl+D on an empty terminal
line to signal EOF and exit. Ctrl+C interrupts the process; Ctrl+Z suspends it.

## How it works

The program sets defaults, scans the command-line arguments, opens the requested
streams, and reads and writes one byte at a time. It does not buffer whole lines,
so there is no fixed line-length limit.

| Argument | Effect |
| --- | --- |
| No key | Copy input unchanged. |
| `+e12345` | Add the cyclic key digits to lowercase letters and digits. |
| `-e4321` | Subtract the cyclic key digits. |
| `-Ifilename` | Read from the named file instead of stdin. |
| `-Ofilename` | Write to the named file instead of stdout, replacing its contents. |
| `-D` | Disable debug logging starting with the next argument. |
| `+D` | Enable debug logging starting with the next argument. |

Options can appear in any order. Supply at most one key, one input file, and one
output file. Keys must contain a non-empty sequence of decimal digits. File names
must immediately follow `-I` or `-O`; quote the entire argument for paths with
spaces. Use different input and output files because opening an output file
truncates it.

Encoding wraps within `a`–`z` and `0`–`9`: `z + 1` becomes `a`, and `0 - 1`
becomes `9`. Uppercase letters, punctuation, whitespace, and other bytes stay
unchanged. **Every input byte advances the key**, including newlines and unchanged
characters. The key cycles continuously across lines.

Debug mode starts enabled. Each argument is printed to stderr if debugging was
enabled before handling that argument. Consequently, an initial `-D` is itself
logged. Stdout (or the selected output file) contains only input-derived bytes,
with no prompts or status messages. Errors go to stderr and produce a nonzero
exit status; normal completion returns zero.

Examples from the repository root:

```sh
# Pass input through unchanged.
printf 'Hello 123!\n' | ./LabA/encoder

# Decode the assignment example.
printf 'gduqp523\n' | ./LabA/encoder -e4321
# stdout: caspl202

# Encode a file, then decode it to a separate file.
./LabA/encoder -D +e12345 -Iinput.txt -Oencoded.txt
./LabA/encoder -D -e12345 -Iencoded.txt -Odecoded.txt
cmp input.txt decoded.txt
```

## Validation and fixes

```sh
make test
make clean
```

The eight automated tests cover both assignment examples, default copying, empty
input, wraparound, key advancement across unchanged bytes, all byte values, long
lines, encoding/decoding round trips, debug transitions, file redirection, all 24
orderings of four options, invalid arguments, and file/output failures.

Corrections include removing the stdout prompt and forbidden `strlen` call,
implementing no-key copying, correcting key advancement and debug timing,
validating arguments, and checking stream errors. The encoder builds independently
of the unrelated Part 0 C/assembly files.

The default build uses the host architecture. For the assignment's 32-bit Linux
target, install your distribution's 32-bit C development support, then run:

```sh
make clean
make encoder CFLAGS='-std=c99 -g -Wall -Wextra -Wpedantic -m32'
```

The host build and tests were verified in this workspace. The 32-bit build could
not be verified because the installed toolchain lacks the 32-bit C headers.

## Files and submission

- `LabA_Tasks`: original assignment requirements.
- `LabA/encoder.c`: assignment implementation.
- `LabA/Makefile`: standalone encoder build.
- `LabA/test_encoder.py`: regression tests.
- `Makefile`: forwards root-level build, test, and clean commands to `LabA/`.
- `main.c`, `numbers.c`, `add.s`, and the copies of `numbers.c`/`add.s` in
  `LabA/`: original Part 0 preparation examples. They are not used by the encoder
  and were not repaired or validated as a separate program. The supplied number
  reader has unchecked input lengths/EOF, so it should not be treated as robust.
- `LabA/output`: pre-existing output artifact, not required for the build.

The task requires a ZIP containing exactly `encoder.c` and lowercase `makefile`,
without a containing directory. To package the encoder from the repository root:

```sh
staging=$(mktemp -d)
cp LabA/encoder.c "$staging/encoder.c"
cp LabA/Makefile "$staging/makefile"
(cd "$staging" && zip labA.zip encoder.c makefile)
cp "$staging/labA.zip" ./labA.zip
```

In a fresh extracted submission directory, `make encoder` produces `./encoder`.
The submission intentionally omits tests; `make test` is available in this
workspace, where `test_encoder.py` is present.
