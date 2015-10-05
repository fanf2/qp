// popcount-test.c: test popcount16() and popcount16x2()
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "qp.h"

int
main(void) {
	srandomdev();
	Trie tt = {{0,0}}, *t = &tt;
	for(;;) {
		long r = random();
		uint b16 = t->branch.bitmap = r & 0xFFFF;
		uint b = (r >> 16) & 0xF;
		uint pc = popcount(b16);
		uint po = popcount(b16 & (b-1));
		uint off = twigoff(t, b);
		uint s, m; TWIGOFFMAX(s, m, t, b);
		if(pc != m || po != s || po != off) {
			printf("%04x b=%d pc=%d po=%d off=%d s=%d m=%d\n",
			       b16, b, pc, po, off, s, m);
			assert(pc == m);
			assert(po == s);
			assert(po == off);
		}
	}
}
