// Tbl-crit-bit.c: tables implemented with crit-bit tries.
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
#include "Tbl-crit-bit.h"

bool
Tgetkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(false);
	Trie *t = &tbl->root;
	while(isbranch(t))
		t = twig(t, twigoff(t, key, len));
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
		for(uint b = twigoff(t, *pkey, *plen); b <= 1; b++)
			if(next_rec(twig(t, b), pkey, plen, pval))
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
	uint b = 0;
	while(isbranch(t)) {
		b = twigoff(t, key, len);
		p = t, t = twig(t, b);
	}
	if(strcmp(key, t->leaf.key) != 0)
		return(tbl);
	*pkey = t->leaf.key;
	*pval = t->leaf.val;
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	// Move the other twig to the parent branch.
	t = p->branch.twigs;
	*p = *twig(p, !b);
	free(t);
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
		return(Tdel(tbl, key));
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
	while(isbranch(t))
		t = twig(t, twigoff(t, key, len));
	// Do the keys differ, and if so, where?
	size_t i;
	for(i = 0; i <= len; i++) {
		if(key[i] != t->leaf.key[i])
			goto newkey;
	}
	t->leaf.val = val;
	return(tbl);
newkey:; // We have the byte index; what about the bit?
	byte k1 = (byte)key[i], k2 = (byte)t->leaf.key[i];
	uint b = 8 - fls(k1 ^ k2);
	i = 8 * i + b;
	b = k1 >> (7 - b) & 1;
	// Find where to insert a branch or grow an existing branch.
	t = &tbl->root;
	while(isbranch(t)) {
		if(i < t->branch.index)
			goto newbranch;
		t = twig(t, twigoff(t, key, len));
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	Trie t1 = { .leaf = { .key = key, .val = val } };
	Trie t2 = *t; // Save before overwriting.
	t->branch.twigs = twigs;
	t->branch.isbranch = 1;
	t->branch.index = i;
	*twig(t, b) = t1;
	*twig(t, !b) = t2;
	return(tbl);
}
