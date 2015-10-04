CFLAGS= -O2 -g -Weverything -Wshadow

all: test-cb test-qp bench-cb bench-qp

test: all
	./test-once.sh 10000 100000 /usr/share/dict/words

bench: all
	./bench-multi.pl ./bench-cb ./bench-qp -- 1000000 /usr/share/dict/words

clean:
	rm -f ${TARG} *.o

realclean: clean
	rm -f test-in test-out-??

bench-cb: bench.o Tbl.o cb.o
	${CC} ${CFLAGS} -o $@ $^

bench-qp: bench.o Tbl.o qp.o
	${CC} ${CFLAGS} -o $@ $^

test-cb: test.o Tbl.o cb.o cb-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qp: test.o Tbl.o qp.o qp-debug.o
	${CC} ${CFLAGS} -o $@ $^

Tbl.o: Tbl.c Tbl.h
test.o: test.c Tbl.h
bench.o: bench.c Tbl.h
cb.o: cb.c cb.h Tbl.h
qp.o: qp.c qp.h Tbl.h
cb-debug.o: cb-debug.c cb.h Tbl.h
qp-debug.o: qp-debug.c qp.h Tbl.h
