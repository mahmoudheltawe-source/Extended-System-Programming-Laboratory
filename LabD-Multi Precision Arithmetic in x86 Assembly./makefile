NASM = nasm
CC = gcc
NASMFLAGS = -f elf32 -Wall -Werror
CPPFLAGS =
CFLAGS = -m32 -O2 -Wall -Wextra -Werror -fno-pie
LDFLAGS = -m32 -no-pie
LDLIBS =
ID ?= submission

.PHONY: all clean test submission

all: multi

multi: multi.o
	$(CC) $(LDFLAGS) $< -o $@ $(LDLIBS)

multi.o: multi.s
	$(NASM) $(NASMFLAGS) $< -o $@

# Part 0 uses an alternate main in the same assembly source.
part0: part0.o
	$(CC) $(LDFLAGS) $< -o $@ $(LDLIBS)

part0.o: multi.s
	$(NASM) $(NASMFLAGS) -DPART0=1 $< -o $@

test: multi part0 tests/test_functions
	./tests/test_functions
	python3 tests/test_multi.py

tests/multi-library.o: multi.s
	$(NASM) $(NASMFLAGS) -DMULTI_LIBRARY=1 $< -o $@

tests/test_functions.o: tests/test_functions.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

tests/test_functions: tests/test_functions.o tests/multi-library.o
	$(CC) $(LDFLAGS) $^ -Wl,--wrap=malloc,--wrap=free -o $@ $(LDLIBS)

# The submitted archive contains exactly the two requested files.
# Usage: make submission ID=123456789
submission: multi.s makefile
	python3 -c 'import sys, zipfile; z = zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED); z.write("multi.s"); z.write("makefile"); z.close()' "$(ID).zip"

clean:
	rm -f multi multi.o part0 part0.o tests/multi-library.o tests/test_functions.o tests/test_functions
