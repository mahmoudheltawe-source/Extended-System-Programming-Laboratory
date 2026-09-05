CC = gcc
ARCH_FLAGS ?= -m32
CFLAGS = -std=c11 -g -O0 -Wall -Wextra -Wpedantic -fno-pie -fno-stack-protector
LDFLAGS = -no-pie

.PHONY: all clean submission
all: hexeditplus Task4

hexeditplus: task1.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

Task4: task4.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

submission: task1.c task4.c makefile
	zip -j lab4-submission.zip task1.c task4.c makefile

clean:
	rm -f hexeditplus Task4 *.o lab4-submission.zip
