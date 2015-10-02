CFLAGS= -g -fsanitize=address -Weverything

all: Tbl-test

clean:
	rm -f Tbl-test *.o

Tbl-test: Tbl-test.o Tbl.o Tbl-qpp-trie.o Tbl-qpp-debug.o
	${CC} ${CFLAGS} -o $@ $^

Tbl-test.o: Tbl-test.c Tbl.h

Tbl.o: Tbl.c Tbl.h

Tbl-qpp-debug.o: Tbl-qpp-debug.c Tbl-qpp-trie.h Tbl.h

Tbl-qpp-trie.o: Tbl-qpp-trie.c Tbl-qpp-trie.h Tbl.h
