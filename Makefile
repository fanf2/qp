CFLAGS= -O2 -g -Weverything

all: Tbl-test

clean:
	rm -f Tbl-test *.o

Tbl-test: Tbl-test.o Tbl-qpp-trie.o
	${CC} ${CFLAGS} -o $@ $^

Tbl-test.o: Tbl-test.c Tbl.h
	${CC} ${CFLAGS} -c $<

Tbl-qpp-trie.o: Tbl-qpp-trie.c Tbl.c Tbl.h
	${CC} ${CFLAGS} -c $<
