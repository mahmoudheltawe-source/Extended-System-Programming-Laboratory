CC = gcc
LD = ld
NASM = nasm
CFLAGS = -m32 -std=c11 -g -Wall -Wextra -Werror -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin

.PHONY: all clean test
all: my_loader

my_loader: my_loader.o startup.o start.o linking_script
	$(LD) -m elf_i386 -T linking_script -o $@ my_loader.o startup.o start.o

my_loader.o: my_loader.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(NASM) -f elf32 $< -o $@

test: my_loader
	python3 tests/test_loader.py

clean:
	rm -f my_loader.o startup.o start.o my_loader
