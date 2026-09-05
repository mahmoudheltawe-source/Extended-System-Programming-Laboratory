CC = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic

.PHONY: all clean test
all: myshell mypipeline looper

test: all
	python3 tests/test_lab.py

myshell: myshell.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

mypipeline: mypipeline.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

looper: looper.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

clean:
	rm -f *.o myshell mypipeline looper
