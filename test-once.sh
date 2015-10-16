#!/bin/sh
set -e
if [ ! -f test-in ]
then	printf 1>&2 "generating..."
	./test-gen.pl $1 $2 $3 >test-in
	printf 1>&2 "done\n"
fi
shift 3
time ./test.pl <test-in >test-out-pl
for i in "$@"
do time ./test-$i <test-in >test-out-$i
done
for i in "$@"
do cmp test-out-pl test-out-$i
done
rm -f test-in test-out-??
