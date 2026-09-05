.PHONY: all run clean
all: run_program
run: run_program
run_program: start.o main.o
	ld -m elf_i386 -o $@ $^
start.o: start.s
	nasm -f elf32 $< -o $@
main.o: main.c
	gcc -m32 -Wall -Wextra -ansi -pedantic -ffreestanding -fno-pie -fno-stack-protector -nostdlib -c $< -o $@
clean:
	rm -f start.o main.o Util.o run_program
