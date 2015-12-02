# You may need -mpopcnt to get the compiler to emit POPCNT instructions
CFLAGS= -O3 -std=gnu99 -Wall -Wextra

# implementation codes
XY=	cb qp qs qn fp fs wp ws # ht
TEST=	$(addprefix ./test-,${XY})
BENCH=  $(addprefix ./bench-,${XY})

INPUT=	in-b9 in-dns in-rdns in-usdw top-1m

all: ${TEST} ${BENCH} ${INPUT}

test: ${TEST} top-1m
	./test-once.sh 10000 100000 top-1m ${XY}

bench: ${BENCH} ${INPUT}
	./bench-more.pl 1000000 ${BENCH} -- ${INPUT}

size: ${TEST} ${INPUT}
	for f in ${INPUT}; do \
		sed 's/^/+/' <$$f >test-$$f; \
		echo $$f; \
		for p in ${TEST}; do \
			$$p <test-$$f >/dev/null; \
		done; \
	done

clean:
	rm -f test-?? bench-?? *.o

realclean: clean
	rm -f test-in test-out-??

bench-ht: bench.o Tbl.o ht.o siphash24.o
	${CC} ${CFLAGS} -o $@ $^

test-ht: test.o Tbl.o ht.o ht-debug.o siphash24.o
	${CC} ${CFLAGS} -o $@ $^

bench-%: bench.o Tbl.o %.o
	${CC} ${CFLAGS} -o $@ $^

test-%: test.o Tbl.o %.o %-debug.o
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

# use SWAR 16 bit x 2 popcount
qn.o: qp.c qp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_NARROW_CPU -c -o qn.o $<

# use hand coded 16 bit popcount
qs.o: qp.c qp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o qs.o $<

# use hand coded 32 bit popcount
fs.o: fp.c fp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o fs.o $<

# use hand coded 64 bit popcount
ws.o: wp.c wp.h Tbl.h
	${CC} ${CFLAGS} -DHAVE_SLOW_POPCOUNT -c -o ws.o $<

qn-debug.c:
	ln -s qp-debug.c qn-debug.c
qs-debug.c:
	ln -s qp-debug.c qs-debug.c
fs-debug.c:
	ln -s fp-debug.c fs-debug.c
ws-debug.c:
	ln -s wp-debug.c ws-debug.c

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

in-rdns: in-dns
	rev in-dns >in-rdns

in-dns:
	for z in cam.ac.uk private.cam.ac.uk \
		eng.cam.ac.uk cl.cam.ac.uk \
		maths.cam.ac.uk damtp.cam.ac.uk dpmms.cam.ac.uk; \
	do dig axfr $$z @131.111.8.37; done |\
	sed '/^;/d;s/[ 	].*//' | uniq >in-dns

in-b9: bind9
	find bind9/ -name '*.c' -o -name '*.h' | \
	xargs ./getwords.pl >in-b9

tex:
	pdflatex tinytocs.tex
	bibtex tinytocs
	pdflatex tinytocs.tex
	pdflatex tinytocs.tex

bind9:
	git clone https://source.isc.org/git/bind9.git

html:
	for f in *.md; do markdown <$$f >$${f%md}html; done

upload: html
	git push chiark:public-git/qp.git
	git push git@github.com:fanf2/qp.git
	git push ucs@git.csx.cam.ac.uk:u/fanf2/radish.git
	ssh chiark public-html/prog/qp/.htupdate
