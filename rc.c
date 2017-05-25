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
	for(;;) {
		// Prefetch the pointer (might be twigs or key) before
		// looking at the index, to discourage the compiler
		// from delaying the prefetch
		__builtin_prefetch(t->ptr);
		Tindex i = t->index;
		if(!Tindex_branch(i))
			break;
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

static bool
next_rec(Trie *t, const char **pkey, size_t *plen, void **pval) {
	Tindex i = t->index;
	if(Tindex_branch(i)) {
		// Recurse to find either this leaf (*pkey != NULL)
		// or the next one (*pkey == NULL).
		Tbitmap b = twigbit(i, *pkey, *plen);
		uint s, m; TWIGOFFMAX(s, m, i, b);
		for(; s < m; s++)
			if(next_rec(Tbranch_twigs(t)+s, pkey, plen, pval))
				return(true);
		return(false);
	}
	// We have found the next leaf.
	if(*pkey == NULL) {
		*pkey = Tleaf_key(t);
		*plen = strlen(*pkey);
		*pval = Tleaf_val(t);
		return(true);
	}
	// We have found this leaf, so start looking for the next one.
	if(strcmp(*pkey, Tleaf_key(t)) == 0) {
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
	return(next_rec(tbl, pkey, plen, pval));
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(NULL);
	Trie *t = tbl, *p = NULL;
	Tbitmap b = 0;
	Tindex i = 0;
	for(;;) {
		__builtin_prefetch(t->ptr);
		i = t->index;
		if(!Tindex_branch(i))
			break;
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
	i = Tbitmap_del(&p->index, b);
	// We have now correctly removed the twig from the trie, so if
	// realloc() fails we can ignore it and continue to use the
	// slightly oversized twig array.
	t = realloc(t, sizeof(Trie) * (m - 1));
	if(t != NULL) Tset_twigs(p, t);
	return(tbl);
}

Tbl *
Tsetl(Tbl *tbl, const char *key, size_t len, void *val) {
	if(Tindex_branch((Tindex)val) || len > Tmaxlen) {
		errno = EINVAL;
		return(NULL);
	}
	if(val == NULL)
		return(Tdell(tbl, key, len));
	// First leaf in an empty tbl?
	if(tbl == NULL) {
		tbl = malloc(sizeof(*tbl));
		if(tbl == NULL) return(NULL);
		Tset_key(tbl, key);
		Tset_val(tbl, val);
		return(tbl);
	}
	Trie *t = tbl;
	// Find the most similar leaf node in the trie. We will compare
	// its key with our new key to find the first differing nibble,
	// which can be at a lower index than the point at which we
	// detect a difference.
	for(;;) {
		__builtin_prefetch(t->ptr);
		Tindex i = t->index;
		if(!Tindex_branch(i))
			break;
		Tbitmap b = twigbit(i, key, len);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigoff(t, b) can cause
		// an out-of-bounds index if it equals twigmax(t).
		uint s = hastwig(i, b) ? twigoff(i, b) : 0;
		t = Tbranch_twigs(t) + s;
	}
	// Do the keys differ, and if so, where?
	uint off, xor, shf;
	const char *tkey = Tleaf_key(t);
	for(off = 0; off <= len; off++) {
		xor = (byte)key[off] ^ (byte)tkey[off];
		if(xor != 0) goto newkey;
	}
	Tset_val(t, val);
	return(tbl);
newkey:; // We have the branch's byte index; what is its chunk index?
	uint bit = off * 8 + (uint)__builtin_clz(xor) + 8 - sizeof(uint) * 8;
	uint qo = bit / 5;
	off = qo * 5 / 8;
	shf = qo * 5 % 8;
	// re-index keys with adjusted offset
	uint k1 = word_up(key+off);
	uint k2 = word_up(tkey+off);
	Tbitmap b1 = nibbit(k1, shf);
	// Prepare the new leaf.
	Trie t1;
	Tset_key(&t1, key);
	Tset_val(&t1, val);
	// Find where to insert a branch or grow an existing branch.
	t = tbl;
	Tindex i = 0;
	for(;;) {
		__builtin_prefetch(t->ptr);
		i = t->index;
		if(!Tindex_branch(i))
			break;
		if(off == Tindex_offset(i) && shf == Tindex_shift(i))
			goto growbranch;
		if(off == Tindex_offset(i) && shf < Tindex_shift(i))
			goto newbranch;
		if(off < Tindex_offset(i))
			goto newbranch;
		Tbitmap b = twigbit(i, key, len);
		assert(hastwig(i, b));
		t = Tbranch_twigs(t) + twigoff(i, b);
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	Trie t2 = *t; // Save before overwriting.
	Tbitmap b2 = nibbit(k2, shf);
	Tset_twigs(t, twigs);
	i = Tindex_set(&t->index, shf, off, b1 | b2);
	twigs[twigoff(i, b1)] = t1;
	twigs[twigoff(i, b2)] = t2;
	return(tbl);
growbranch:;
	assert(!hastwig(i, b1));
	uint s, m; TWIGOFFMAX(s, m, i, b1);
	twigs = realloc(Tbranch_twigs(t), sizeof(Trie) * (m + 1));
	if(twigs == NULL) return(NULL);
	memmove(twigs+s+1, twigs+s, sizeof(Trie) * (m - s));
	memmove(twigs+s, &t1, sizeof(Trie));
	Tset_twigs(t, twigs);
	i = Tbitmap_add(&t->index, b1);
	return(tbl);
}
