CFLAGS= -g -fsanitize=address -Weverything

TARG=	test-cb test-qp

all: ${TARG}

clean:
	rm -f ${TARG} *.o

realclean: clean
	rm -f test-in test-out-??

test-cb: test.o Tbl.o cb.o cb-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qp: test.o Tbl.o qp.o qp-debug.o
	${CC} ${CFLAGS} -o $@ $^

Tbl.o: Tbl.c Tbl.h
test.o: test.c Tbl.h
cb.o: cb.c cb.h Tbl.h
qp.o: qp.c qp.h Tbl.h
cb-debug.o: cb-debug.c cb.h Tbl.h
qp-debug.o: qp-debug.c qp.h Tbl.h
