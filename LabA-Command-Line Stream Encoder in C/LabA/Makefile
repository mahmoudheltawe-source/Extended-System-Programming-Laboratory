CC = gcc
CFLAGS = -std=c99 -g -Wall -Wextra -Wpedantic

.PHONY: all clean test
all: encoder

encoder: encoder.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

test: encoder
	python3 test_encoder.py

clean:
	rm -f encoder *.o
