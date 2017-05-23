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
		Tbitmap b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(false);
		t = twig(t, twigoff(t, b));
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
	while(isbranch(t)) {
		__builtin_prefetch(Tbranch_twigs(t));
		b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(tbl);
		p = t; t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return(tbl);
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	t = p; p = NULL; // Becuase t is the usual name
	uint s, m; TWIGOFFMAX(s, m, t, b);
	if(m == 2) {
		// Move the other twig to the parent branch.
		p = Tbranch_twigs(t);
		*t = *twig(t, !s);
		free(p);
		return(tbl);
	}
	memmove(twig(t,s), twig(t,s+1), sizeof(Trie) * (m - s - 1));
	Tbitmap_del(t,b);
	// We have now correctly removed the twig from the trie, so if
	// realloc() fails we can ignore it and continue to use the
	// slightly oversized twig array.
	p = realloc(Tbranch_twigs(t), sizeof(Trie) * (m - 1));
	if(p != NULL) Tset_twigs(t, p);
	return(tbl);
}
