#!/bin/sh
set -e
[ -f Tbl-test-input ] || ./Tbl-test-generate.pl "$@" >Tbl-test-input
./Tbl-test.pl <Tbl-test-input >Tbl-test-out-pl
./Tbl-test <Tbl-test-input >Tbl-test-out-c
cmp Tbl-test-out-pl Tbl-test-out-c
rm -f Tbl-test-input Tbl-test-out-pl Tbl-test-out-c
