CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11

.PHONY: all clean
all: myshell mypipe looper

myshell: myshell.o LineParser.o
	$(CC) $(CFLAGS) $^ -o $@

myshell.o LineParser.o: LineParser.h

mypipe: mypipe.o
	$(CC) $(CFLAGS) $^ -o $@

looper: looper.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o myshell mypipe looper
