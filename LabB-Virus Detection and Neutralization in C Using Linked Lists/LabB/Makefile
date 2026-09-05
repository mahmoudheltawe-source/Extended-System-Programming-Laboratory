CC = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic

.PHONY: all preparation clean
all: AntiVirus

preparation: Bubblesort hexaPrint

AntiVirus: AntiVirus.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

Bubblesort: Bubblesort.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

hexaPrint: hexaPrint.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	$(RM) AntiVirus Bubblesort hexaPrint *.o
