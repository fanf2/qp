// qp.c: tables implemented with quadbit popcount patricia tries.
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
#include "qp.h"

bool
Tgetkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(false);
	Trie *t = &tbl->root;
	while(isbranch(t)) {
		__builtin_prefetch(t->branch.twigs);
		Tbitmap b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(false);
		t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, t->leaf.key) != 0)
		return(false);
	*pkey = t->leaf.key;
	*pval = t->leaf.val;
	return(true);
}

static bool
next_rec(Trie *t, const char **pkey, size_t *plen, void **pval) {
	if(isbranch(t)) {
		// Recurse to find either this leaf (*pkey != NULL)
		// or the next one (*pkey == NULL).
		Tbitmap b = twigbit(t, *pkey, *plen);
		uint s, m; TWIGOFFMAX(s, m, t, b);
		for(; s < m; s++)
			if(next_rec(twig(t, s), pkey, plen, pval))
				return(true);
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
	return(next_rec(&tbl->root, pkey, plen, pval));
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(NULL);
	Trie *t = &tbl->root, *p = NULL;
	Tbitmap b = 0;
	while(isbranch(t)) {
		__builtin_prefetch(t->branch.twigs);
		b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(tbl);
		p = t; t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, t->leaf.key) != 0)
		return(tbl);
	*pkey = t->leaf.key;
	*pval = t->leaf.val;
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	t = p; p = NULL; // Becuase t is the usual name
	uint s, m; TWIGOFFMAX(s, m, t, b);
	if(m == 2) {
		// Move the other twig to the parent branch.
		Trie *twigs = t->branch.twigs;
		*t = *twig(t, !s);
		free(twigs);
		return(tbl);
	}
	memmove(t->branch.twigs+s, t->branch.twigs+s+1, sizeof(Trie) * (m - s - 1));
	t->branch.bitmap &= ~b;
	// We have now correctly removed the twig from the trie, so if
	// realloc() fails we can ignore it and continue to use the
	// slightly oversized twig array.
	Trie *twigs = realloc(t->branch.twigs, sizeof(Trie) * (m - 1));
	if(twigs != NULL) t->branch.twigs = twigs;
	return(tbl);
}

Tbl *
Tsetl(Tbl *tbl, const char *key, size_t len, void *val) {
	// Ensure flag bits are zero.
	if(((uint64_t)val & 3) != 0) {
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
	// Find the most similar leaf node in the trie. We will compare
	// its key with our new key to find the first differing nibble,
	// which can be at a lower index than the point at which we
	// detect a difference.
	while(isbranch(t)) {
		__builtin_prefetch(t->branch.twigs);
		Tbitmap b = twigbit(t, key, len);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigoff(t, b) can cause
		// an out-of-bounds index if it equals twigmax(t).
		uint i = hastwig(t, b) ? twigoff(t, b) : 0;
		t = twig(t, i);
	}
	// Do the keys differ, and if so, where?
	size_t i;
	for(i = 0; i <= len; i++) {
		if(key[i] != t->leaf.key[i])
			goto newkey;
	}
	t->leaf.val = val;
	return(tbl);
newkey:; // We have the branch's index; what are its flags?
	byte k1 = (byte)key[i], k2 = (byte)t->leaf.key[i];
	uint f =  k1 ^ k2;
	f = (f & 0xf0) ? 1 : 2;
	// Prepare the new leaf.
	Tbitmap b1 = nibbit(k1, f);
	Trie t1 = { .leaf = { .key = key, .val = val } };
	// Find where to insert a branch or grow an existing branch.
	t = &tbl->root;
	while(isbranch(t)) {
		__builtin_prefetch(t->branch.twigs);
		if(i == t->branch.index && f == t->branch.flags)
			goto growbranch;
		if(i == t->branch.index && f < t->branch.flags)
			goto newbranch;
		if(i < t->branch.index)
			goto newbranch;
		Tbitmap b = twigbit(t, key, len);
		assert(hastwig(t, b));
		t = twig(t, twigoff(t, b));
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	Trie t2 = *t; // Save before overwriting.
	Tbitmap b2 = nibbit(k2, f);
	t->branch.twigs = twigs;
	t->branch.flags = f;
	t->branch.index = i;
	t->branch.bitmap = b1 | b2;
	*twig(t, twigoff(t, b1)) = t1;
	*twig(t, twigoff(t, b2)) = t2;
	return(tbl);
growbranch:;
	assert(!hastwig(t, b1));
	uint s, m; TWIGOFFMAX(s, m, t, b1);
	twigs = realloc(t->branch.twigs, sizeof(Trie) * (m + 1));
	if(twigs == NULL) return(NULL);
	memmove(twigs+s+1, twigs+s, sizeof(Trie) * (m - s));
	memmove(twigs+s, &t1, sizeof(Trie));
	t->branch.twigs = twigs;
	t->branch.bitmap |= b1;
	return(tbl);
}
