// qp.c: tables implemented with hash array mapped tries
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
#include "ht.h"

static inline uint64_t
hash(const char *key, size_t len, uint depth) {
	uint64_t h, stir[2] = { depth, depth };
	siphash((void*)&h, (const void *)key, len, (void*)stir);
	return(h);
}

bool
Tgetkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(false);
	Trie *t = &tbl->root;
	for(uint d1 = 0 ;; ++d1) {
		uint64_t h = hash(key, len, d1);
		for(uint d2 = lglgN; d2 < Hbits; d2 += lglgN, h >>= lglgN) {
			if(!isbranch(t))
				goto leaf;
			uintptr_t b = twigbit(h);
			if(!hastwig(t, b))
				return(false);
			t = twig(t, twigoff(t, b));
		}
	}
leaf:	if(strcmp(key, t->leaf.key) != 0)
		return(false);
	*pkey = t->leaf.key;
	*pval = t->leaf.val;
	return(true);
}

static bool
next_rec(Trie *t, const char **pkey, size_t *plen, void **pval,
	 uint64_t h, uint d1, uint d2) {
	if(isbranch(t)) {
		if(d2 >= Hbits) {
			h = *pkey == NULL ? 0 :
				hash(*pkey, *plen, d1++);
			d2 = lglgN;
		}
		uintptr_t b = twigbit(h);
		uint s = twigoff(t, b);
		uint m = twigmax(t);
		for(; s < m; s++)
			if(next_rec(twig(t, s), pkey, plen, pval,
				    h >> lglgN, d1, d2 + lglgN))
				return(true);
			else
				h = 0;
		return(false);
	}
	// We have found the next leaf.
	if(*pkey == NULL) {
		*pkey = t->leaf.key;
		*plen = strlen(*pkey);
		*pval = t->leaf.val;
		return(true);
	}
	// We have found this leaf, so start looking for the next one.
	if(strcmp(*pkey, t->leaf.key) == 0) {
		*pkey = NULL;
		*plen = 0;
		return(false);
	}
	// No match.
	return(false);
}

bool
Tnextl(Tbl *tbl, const char **pkey, size_t *plen, void **pval) {
	if(tbl == NULL) {
		*pkey = NULL;
		*plen = 0;
		return(NULL);
	}
	return(next_rec(&tbl->root, pkey, plen, pval, 0, 0, Hbits));
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(NULL);
	Trie *t = &tbl->root, *p = NULL;
	uint64_t h = 0;
	uintptr_t b = 0;
	for(uint d1 = 0 ;; ++d1) {
		h = hash(key, len, d1);
		for(uint d2 = lglgN; d2 < Hbits; d2 += lglgN, h >>= lglgN) {
			if(!isbranch(t))
				goto leaf;
			b = twigbit(h);
			if(!hastwig(t, b))
				return(tbl);
			p = t; t = twig(t, twigoff(t, b));
		}
	}
leaf:	if(strcmp(key, t->leaf.key) != 0)
		return(tbl);
	*pkey = t->leaf.key;
	*pval = t->leaf.val;
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	t = p; p = NULL; // Becuase t is the usual name
	uint s = twigoff(t, b), m = twigmax(t);
	if(m == 2) {
		// Move the other twig to the parent branch.
		// XXX: May need to unsplice deep hash collision here.
		Trie *twigs = twig(t, 0);
		*t = *twig(t, !s);
		free(twigs);
		return(tbl);
	}
	Trie *twigs = malloc(sizeof(Trie) * (m - 1));
	if(twigs == NULL) return(NULL);
	memcpy(twigs, twig(t, 0), sizeof(Trie) * s);
	memcpy(twigs+s, twig(t, s+1), sizeof(Trie) * (m - s - 1));
	free(twig(t, 0));
	twigset(t, twigs);
	t->branch.map &= ~b;
	return(tbl);
}

Tbl *
Tsetl(Tbl *tbl, const char *key, size_t len, void *val) {
	// Ensure flag bits are zero.
	if(((uintptr_t)val & 1) != 0) {
		errno = EINVAL;
		return(NULL);
	}
	if(val == NULL)
		return(Tdell(tbl, key, len));
	// First leaf in an empty tbl?
	if(tbl == NULL) {
		tbl = malloc(sizeof(*tbl));
		if(tbl == NULL) return(NULL);
		tbl->root.leaf.key = key;
		tbl->root.leaf.val = val;
		return(tbl);
	}
	Trie *t = &tbl->root;
	Trie t1 = { .leaf = { .key = key, .val = val } };
	uint d1, d2;
	uintptr_t b1;
	for(d1 = 0 ;; ++d1) {
		uint64_t h = hash(key, len, d1);
		for(d2 = lglgN; d2 < Hbits; d2 += lglgN, h >>= lglgN) {
			b1 = twigbit(h);
			if(!isbranch(t))
				goto leaf;
			if(!hastwig(t, b1))
				goto growbranch;
			t = twig(t, twigoff(t, b1));
		}
	}
leaf:	if(strcmp(key, t->leaf.key) != 0)
		goto newbranch;
	t->leaf.val = val;
	return(tbl);
newbranch:;
	// XXX May need multiple levels of trie here.
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	Trie t2 = *t; // Save before overwriting.
	uint64_t h2 = hash(t->leaf.key, strlen(t->leaf.key), d1);
	uint64_t b2 = twigbit(h2 >>= d2 - lglgN);
	t->branch.map = b1 | b2;
	twigset(t, twigs);
	*twig(t, twigoff(t, b1)) = t1;
	*twig(t, twigoff(t, b2)) = t2;
	return(tbl);
growbranch:;
	assert(!hastwig(t, b1));
	uint s = twigoff(t, b1), m = twigmax(t);
	twigs = malloc(sizeof(Trie) * (m + 1));
	if(twigs == NULL) return(NULL);
	memcpy(twigs, twig(t, 0), sizeof(Trie) * s);
	memcpy(twigs+s, &t1, sizeof(Trie));
	memcpy(twigs+s+1, twig(t, s), sizeof(Trie) * (m - s));
	free(twig(t, 0));
	twigset(t, twigs);
	t->branch.map |= b1;
	return(tbl);
}
