// ht-debug.c: HAMT debug support
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "Tbl.h"
#include "ht.h"

static void
dump_rec(Trie *t, int d) {
	if(isbranch(t)) {
		printf("Tdump%*s branch %p\n", d, "", t);
		for(uint i = 0; i < 64; i++) {
			uint64_t b = twigbit(i);
			if(hastwig(t, b)) {
				printf("Tdump%*s twig %d\n", d, "", i);
				dump_rec(twig(t, twigoff(t, b)), d+1);
			}
		}
	} else {
		printf("Tdump%*s leaf %p\n", d, "", t);
		printf("Tdump%*s leaf key %p %s\n", d, "",
		       t->key, t->key);
		printf("Tdump%*s leaf val %p\n", d, "",
		       t->val);
	}
}

void
Tdump(Tbl *tbl) {
	printf("Tdump root %p\n", tbl);
	if(tbl != NULL)
		dump_rec(tbl, 0);
}

static void
size_rec(Trie *t, uint d, size_t *rsize, size_t *rdepth, size_t *rleaves) {
	*rsize += sizeof(*t);
	if(isbranch(t)) {
		for(uint i = 0; i < 64; i++) {
			uint64_t b = twigbit(i);
			if(hastwig(t, b))
				size_rec(twig(t, twigoff(t, b)), d+1,
					 rsize, rdepth, rleaves);
		}
	} else {
		*rdepth += d;
		*rleaves += 1;
	}
}

void
Tsize(Tbl *tbl, const char **rtype,
      size_t *rsize, size_t *rdepth, size_t *rleaves) {
	*rtype = "ht";
	*rsize = *rdepth = *rleaves = 0;
	if(tbl != NULL)
		size_rec(tbl, 0, rsize, rdepth, rleaves);
}
