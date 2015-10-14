#!/bin/sh
set -e
if [ ! -f test-in ]
then	printf 1>&2 "generating..."
	./test-gen.pl "$@" >test-in
	printf 1>&2 "done\n"
fi
time ./test.pl <test-in >test-out-pl
time ./test-cb <test-in >test-out-cb
time ./test-qp <test-in >test-out-qp
time ./test-qn <test-in >test-out-qn
time ./test-qs <test-in >test-out-qs
time ./test-wp <test-in >test-out-wp
#time ./test-ht <test-in | sort >test-out-ht
cmp test-out-pl test-out-cb
cmp test-out-pl test-out-qp
cmp test-out-pl test-out-qn
cmp test-out-pl test-out-qs
cmp test-out-pl test-out-wp
#cmp test-out-pl test-out-ht
rm -f test-in test-out-??
