#!/bin/sh
set -e
[ -f Tbl-test-input ] || ./Tbl-test-generate.pl "$@" >Tbl-test-input
./Tbl-test.pl  <Tbl-test-input >Tbl-test-out-pl
time ./Tbl-test-qpp <Tbl-test-input >Tbl-test-out-qpp
time ./Tbl-test-cb  <Tbl-test-input >Tbl-test-out-cb
cmp Tbl-test-out-pl Tbl-test-out-qpp
cmp Tbl-test-out-pl Tbl-test-out-cb
rm -f Tbl-test-input Tbl-test-out-pl Tbl-test-out-qpp Tbl-test-out-cb
