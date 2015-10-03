CFLAGS= -g -fsanitize=address -Weverything

all: Tbl-test-cb Tbl-test-qpp

clean:
	rm -f Tbl-test *.o

Tbl-test-cb: Tbl-test.o Tbl.o Tbl-crit-bit.o Tbl-crit-bit-debug.o
	${CC} ${CFLAGS} -o $@ $^

Tbl-test-qpp: Tbl-test.o Tbl.o Tbl-qpp-trie.o Tbl-qpp-debug.o
	${CC} ${CFLAGS} -o $@ $^

Tbl-test.o: Tbl-test.c Tbl.h

Tbl.o: Tbl.c Tbl.h

Tbl-crit-bit-debug.o: Tbl-crit-bit-debug.c Tbl-crit-bit.h Tbl.h

Tbl-crit-bit.o: Tbl-crit-bit.c Tbl-crit-bit.h Tbl.h

Tbl-qpp-debug.o: Tbl-qpp-debug.c Tbl-qpp-trie.h Tbl.h

Tbl-qpp-trie.o: Tbl-qpp-trie.c Tbl-qpp-trie.h Tbl.h
