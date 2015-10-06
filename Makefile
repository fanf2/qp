CFLAGS= -O3 -std=gnu99 -Wall -Wextra

all: test-cb test-qp test-qs bench-cb bench-qp bench-qs # test-ht bench-ht

test: all
	./test-once.sh 10000 100000 /usr/share/dict/words

bench: all
	./bench-multi.pl ./bench-cb ./bench-qp ./bench-qs \
		-- 1000000 /usr/share/dict/words

clean:
	rm -f test-?? bench-?? *.o

realclean: clean
	rm -f test-in test-out-??

bench-cb: bench.o Tbl.o cb.o
	${CC} ${CFLAGS} -o $@ $^

bench-qp: bench.o Tbl.o qp.o
	${CC} ${CFLAGS} -o $@ $^

bench-qs: bench.o Tbl.o qs.o
	${CC} ${CFLAGS} -o $@ $^

bench-ht: bench.o Tbl.o ht.o siphash24.o
	${CC} ${CFLAGS} -o $@ $^

test-cb: test.o Tbl.o cb.o cb-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qp: test.o Tbl.o qp.o qp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qs: test.o Tbl.o qs.o qp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-ht: test.o Tbl.o ht.o ht-debug.o siphash24.o
	${CC} ${CFLAGS} -o $@ $^

Tbl.o: Tbl.c Tbl.h
test.o: test.c Tbl.h
bench.o: bench.c Tbl.h
siphash24.o: siphash24.c
cb.o: cb.c cb.h Tbl.h
qp.o: qp.c qp.h Tbl.h
ht.o: ht.c ht.h Tbl.h
cb-debug.o: cb-debug.c cb.h Tbl.h
qp-debug.o: qp-debug.c qp.h Tbl.h
ht-debug.o: ht-debug.c ht.h Tbl.h

# use hand coded 16 bit popcount
qs.o: qp.c qp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o qs.o $<

README.html: README.md
	markdown $< >$@

upload: README.html
	rsync README.html chiark:public-html/prog/qp/index.html
	git push chiark:public-git/qp.git
	git push git@github.com:fanf2/qp.git
	git push ucs@git.csx.cam.ac.uk:u/fanf2/radish.git
