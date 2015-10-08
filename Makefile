# You may need -mpopcnt to get the compiler to emit POPCNT instructions
CFLAGS= -O3 -std=gnu99 -Wall -Wextra

# implementation codes
XY=	cb qp qs qn #ht
TEST=	$(addprefix ./test-,${XY})
BENCH=  $(addprefix ./bench-,${XY})

all: ${TEST} ${BENCH} top-1m

test: all
	./test-once.sh 10000 100000 top-1m

bench: all
	./bench-multi.pl ${BENCH} -- 1000000 top-1m

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

bench-qn: bench.o Tbl.o qn.o
	${CC} ${CFLAGS} -o $@ $^

bench-ht: bench.o Tbl.o ht.o siphash24.o
	${CC} ${CFLAGS} -o $@ $^

test-cb: test.o Tbl.o cb.o cb-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qp: test.o Tbl.o qp.o qp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qs: test.o Tbl.o qs.o qp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-qn: test.o Tbl.o qn.o qp-debug.o
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

# use SWAR 16 bit x 2 popcount
qn.o: qp.c qp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_NARROW_CPU -c -o qn.o $<

top-1m: top-1m.csv
	sed 's/^[0-9]*,//' <$< >$@
top-1m.csv: top-1m.csv.zip
	rm -f $@
	unzip $<
	touch $@
top-1m.csv.zip:
	curl -O http://s3.amazonaws.com/alexa-static/top-1m.csv.zip

bind9-words: bind9
	find bind9/ -name '*.c' -o -name '*.h' | \
	xargs perl -ne ' \
		$$a{$$_} = 1 for m{\b[A-Za-z0-9_]+\b}g; \
		END { print "$$_\n" for keys %a } \
	' >bind9-words

html:
	for f in *.md; do markdown <$$f >$${f%md}html; done
	[ -h index.html ] || ln README.html index.html

upload: html
	git push chiark:public-git/qp.git
	git push git@github.com:fanf2/qp.git
	git push ucs@git.csx.cam.ac.uk:u/fanf2/radish.git
	ssh chiark public-html/prog/qp/.htupdate
