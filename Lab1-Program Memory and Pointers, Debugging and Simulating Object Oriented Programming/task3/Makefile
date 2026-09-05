all:
	make base
	make count-words
	make addresses

base: base.c
	gcc -g -Wall -c base.c -o base.o
	gcc -g -Wall base.o -o base
	
	rm base.o

count-words: count-words.c
	gcc -g -Wall -c count-words.c -o count-words.o
	gcc -g -Wall count-words.o -o count-words
	
	rm count-words.o

addresses: addresses.c
	gcc -g -Wall -c addresses.c -o addresses.o
	gcc -g -Wall addresses.o -o addresses
	
	rm addresses.o

.PHONY: clean
clean:
	rm -f */.o base count-words addresses
