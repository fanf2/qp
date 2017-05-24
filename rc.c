// rc.h: quintet bit popcount patricia tries, with rib compression
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Tbl.h"
#include "rc.h"

bool
Tgetkv(Tbl *t, const char *key, size_t len, const char **pkey, void **pval) {
	if(t == NULL)
		return(false);
	while(isbranch(t)) {
		__builtin_prefetch(Tbranch_twigs(t));
		Tindex i = t->index;
		Tbitmap b = twigbit(i, key, len);
		if(!hastwig(i, b))
			return(false);
		t = Tbranch_twigs(t) + twigoff(i, b);
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return(false);
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	return(true);
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(NULL);
	Trie *t = tbl, *p = NULL;
	Tbitmap b = 0;
	Tindex i = 0;
	while(isbranch(t)) {
		__builtin_prefetch(Tbranch_twigs(t));
		i = t->index;
		b = twigbit(i, key, len);
		if(!hastwig(i, b))
			return(tbl);
		p = t; t = Tbranch_twigs(t) + twigoff(i, b);
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return(tbl);
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	t = Tbranch_twigs(p);
	uint s, m; TWIGOFFMAX(s, m, i, b);
	if(m == 2) {
		// Move the other twig to the parent branch.
		*p = t[!s];
		free(t);
		return(tbl);
	}
	memmove(t+s, t+s+1, sizeof(Trie) * (m - s - 1));
	Tbitmap_del(&p->index, b);
	// We have now correctly removed the twig from the trie, so if
	// realloc() fails we can ignore it and continue to use the
	// slightly oversized twig array.
	t = realloc(t, sizeof(Trie) * (m - 1));
	if(t != NULL) Tset_twigs(p, t);
	return(tbl);
}
