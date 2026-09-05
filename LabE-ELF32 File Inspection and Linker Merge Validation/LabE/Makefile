CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11
ARCH_FLAGS = -m32

.PHONY: all clean
all: myELF

myELF: myELF.c
	$(CC) $(CFLAGS) $(ARCH_FLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -f myELF myELF.o
