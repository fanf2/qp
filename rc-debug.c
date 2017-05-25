// rc-debug.c: rc trie debug support
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
#include "rc.h"

const char *
dump_bitmap(Tbitmap w) {
	static char buf[32*3];
	int size = (int)sizeof(buf), n = 0;
	n += snprintf(buf+n, size-n, "(");
	for(uint s = 0; s < 32; s++) {
		Tbitmap b = 1 << s;
		if(w & b)
			n += snprintf(buf+n, size-n, "%u,", s);
	}
	if(n > 1)
		buf[n-1] = ')';
	return buf;
}

static void
dump_rec(Trie *t, uint d) {
	Tindex i = t->index;
	if(Tindex_branch(i)) {
		printf("Tdump%*s branch %p %s %zu %d\n", d, "", (void*)t,
		       dump_bitmap(Tindex_bitmap(i)),
		       (size_t)Tindex_offset(i), Tindex_shift(i));
		uint dd = 1 + Tindex_offset(i) * 8 + Tindex_shift(i);
		assert(dd > d);
		for(uint s = 0; s < 32; s++) {
			Tbitmap b = 1 << s;
			if(hastwig(i, b)) {
				printf("Tdump%*s twig %d\n", d, "", s);
				dump_rec(Tbranch_twigs(t) + twigoff(i, b), dd);
			}
		}
	} else {
		printf("Tdump%*s leaf %p\n", d, "",
		       (void *)t);
		printf("Tdump%*s leaf key %p %s\n", d, "",
		       (const void *)Tleaf_key(t), Tleaf_key(t));
		printf("Tdump%*s leaf val %p\n", d, "",
		       (void *)Tleaf_val(t));
	}
}

void
Tdump(Tbl *tbl) {
	printf("Tdump root %p\n", (void*)tbl);
	if(tbl != NULL)
		dump_rec(tbl, 0);
}

static void
size_rec(Trie *t, uint d,
    size_t *rsize, size_t *rdepth, size_t *rbranches, size_t *rleaves) {
	*rsize += sizeof(*t);
	Tindex i = t->index;
	if(Tindex_branch(i)) {
		*rbranches += 1;
		for(uint s = 0; s < 32; s++) {
			Tbitmap b = 1 << s;
			if(hastwig(i, b))
				size_rec(Tbranch_twigs(t) + twigoff(i, b),
				    d+1, rsize, rdepth, rbranches, rleaves);
		}
	} else {
		*rleaves += 1;
		*rdepth += d;
	}
}

void
Tsize(Tbl *tbl, const char **rtype,
    size_t *rsize, size_t *rdepth, size_t *rbranches, size_t *rleaves) {
	*rtype = "rc";
	*rsize = *rdepth = *rbranches = *rleaves = 0;
	if(tbl != NULL)
		size_rec(tbl, 0, rsize, rdepth, rbranches, rleaves);
}
