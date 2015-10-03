// cb-debug.c: crit-bit trie debug support
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "Tbl.h"
#include "cb.h"

static void
dump_rec(Trie *t, int d) {
	if(isbranch(t)) {
		printf("Tdump%*s branch %p %llu\n", d, "",
		       t, t->branch.index);
		assert(t->branch.index >= d);
		printf("Tdump%*s twig 0\n", d, "");
		dump_rec(twig(t, 0), t->branch.index + 1);
		printf("Tdump%*s twig 1\n", d, "");
		dump_rec(twig(t, 1), t->branch.index + 1);
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
		size_rec(twig(t, 0), d+1, rsize, rdepth, rleaves);
		size_rec(twig(t, 1), d+1, rsize, rdepth, rleaves);
	} else {
		*rdepth += d;
		*rleaves += 1;
	}
}

void
Tsize(Tbl *tbl, const char **rtype,
      size_t *rsize, size_t *rdepth, size_t *rleaves) {
	*rtype = "cb";
	*rsize = *rdepth = *rleaves = 0;
	if(tbl != NULL)
		size_rec(&tbl->root, 0, rsize, rdepth, rleaves);
}
