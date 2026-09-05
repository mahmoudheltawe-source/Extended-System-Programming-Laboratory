.PHONY: all run clean
all: run_program
run: run_program
run_program: start.o
	ld -m elf_i386 -o $@ $^
start.o: start.s
	nasm -f elf32 $< -o $@
clean:
	rm -f start.o run_program
