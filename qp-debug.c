// Tbl-qpp-debug.c: qpp trie debug support
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "Tbl.h"
#include "Tbl-qpp-trie.h"

static void
dump_rec(Trie *t, int d) {
	if(isbranch(t)) {
		printf("Tdump%*s branch %p %llu %d\n", d, "", t,
		       t->branch.index, t->branch.flags);
		int dd = 2 + t->branch.index * 4 + (t->branch.flags - 1) * 2;
		assert(dd > d);
		for(uint i = 0; i < 16; i++) {
			uint b = 1 << i;
			if(hastwig(t, b)) {
				printf("Tdump%*s twig %d\n", d, "", i);
				dump_rec(twig(t, twigoff(t, b)), dd);
			}
		}
	} else {
		printf("Tdump%*s leaf %p\n", d, "", t);
		printf("Tdump%*s leaf key %p %s\n", d, "",
		       t->leaf.key, t->leaf.key);
		printf("Tdump%*s leaf val %p\n", d, "",
		       t->leaf.val);
	}
}

void
Tdump(Tbl *tbl) {
	printf("Tdump root %p\n", tbl);
	if(tbl != NULL)
		dump_rec(&tbl->root, 0);
}

static void
size_rec(Trie *t, uint d, size_t *rsize, size_t *rdepth, size_t *rleaves) {
	*rsize += sizeof(*t);
	if(isbranch(t)) {
		for(uint i = 0, j = twigmax(t); i < j; i++)
			size_rec(twig(t, i), d+1, rsize, rdepth, rleaves);
	} else {
		*rdepth += d;
		*rleaves += 1;
	}
}

void
Tsize(Tbl *tbl, const char **rtype,
      size_t *rsize, size_t *rdepth, size_t *rleaves) {
	*rtype = "qpp";
	*rsize = *rdepth = *rleaves = 0;
	if(tbl != NULL)
		size_rec(&tbl->root, 0, rsize, rdepth, rleaves);
}
