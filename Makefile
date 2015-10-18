# You may need -mpopcnt to get the compiler to emit POPCNT instructions
CFLAGS= -O3 -std=gnu99 -Wall -Wextra

# implementation codes
XY=	cb qs fp wp # qp qn ht
TEST=	$(addprefix ./test-,${XY})
BENCH=  $(addprefix ./bench-,${XY})

INPUT=	in-b9 in-usdw top-1m

all: ${TEST} ${BENCH} ${INPUT}

test: ${TEST} top-1m
	./test-once.sh 10000 100000 top-1m ${XY}

bench: ${BENCH} ${INPUT}
	./bench-more.pl 1000000 ${BENCH} -- ${INPUT}

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

bench-fp: bench.o Tbl.o fp.o
	${CC} ${CFLAGS} -o $@ $^

bench-fs: bench.o Tbl.o fs.o
	${CC} ${CFLAGS} -o $@ $^

bench-wp: bench.o Tbl.o wp.o
	${CC} ${CFLAGS} -o $@ $^

bench-ws: bench.o Tbl.o ws.o
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

test-fp: test.o Tbl.o fp.o fp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-fs: test.o Tbl.o fs.o fp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-wp: test.o Tbl.o wp.o wp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-ws: test.o Tbl.o ws.o wp-debug.o
	${CC} ${CFLAGS} -o $@ $^

test-ht: test.o Tbl.o ht.o ht-debug.o siphash24.o
	${CC} ${CFLAGS} -o $@ $^

Tbl.o: Tbl.c Tbl.h
test.o: test.c Tbl.h
bench.o: bench.c Tbl.h
siphash24.o: siphash24.c
cb.o: cb.c cb.h Tbl.h
qp.o: qp.c qp.h Tbl.h
fp.o: fp.c fp.h Tbl.h
wp.o: wp.c wp.h Tbl.h
ht.o: ht.c ht.h Tbl.h
cb-debug.o: cb-debug.c cb.h Tbl.h
qp-debug.o: qp-debug.c qp.h Tbl.h
fp-debug.o: fp-debug.c fp.h Tbl.h
wp-debug.o: wp-debug.c wp.h Tbl.h
ht-debug.o: ht-debug.c ht.h Tbl.h

# use hand coded 16 bit popcount
qs.o: qp.c qp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o qs.o $<

# use SWAR 16 bit x 2 popcount
qn.o: qp.c qp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_NARROW_CPU -c -o qn.o $<

# use hand coded 32 bit popcount
fs.o: fp.c fp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o fs.o $<

# use hand coded 64 bit popcount
ws.o: wp.c wp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o ws.o $<

input: ${INPUT}

in-usdw:
	ln -s /usr/share/dict/words in-usdw

top-1m: top-1m.csv
	sed 's/^[0-9]*,//' <$< >$@
top-1m.csv: top-1m.csv.zip
	rm -f $@
	unzip $<
	touch $@
top-1m.csv.zip:
	curl -O http://s3.amazonaws.com/alexa-static/top-1m.csv.zip

in-dns:
	for z in cam.ac.uk private.cam.ac.uk \
		eng.cam.ac.uk cl.cam.ac.uk \
		maths.cam.ac.uk damtp.cam.ac.uk dpmms.cam.ac.uk; \
	do dig axfr $$z @131.111.8.37; done |\
	sed '/^;/d;s/[ 	].*//' | uniq >in-dns

in-b9: bind9
	find bind9/ -name '*.c' -o -name '*.h' | \
	xargs ./getwords.pl >in-b9

bind9:
	git clone https://source.isc.org/git/bind9.git

html:
	for f in *.md; do markdown <$$f >$${f%md}html; done

upload: html
	git push chiark:public-git/qp.git
	git push git@github.com:fanf2/qp.git
	git push ucs@git.csx.cam.ac.uk:u/fanf2/radish.git
	ssh chiark public-html/prog/qp/.htupdate
