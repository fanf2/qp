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
	Tindex i = t->index;
	Trie *twigs = t->ptr;
	__builtin_prefetch(twigs);
	while(Tindex_branch(i)) {
		byte n = nibble(i, key, len);
		Tbitmap b = 1U << n;
		if(hastwig(i, b)) {
			t = twigs + twigoff(i, b);
			i = t->index;
			twigs = t->ptr;
			__builtin_prefetch(twigs);
		} else if(Tindex_concat(i) == n) {
			uint max = popcount(Tindex_bitmap(i));
			Tindex *ip = (void*)(twigs + max);
			t = NULL;
			i = *ip++;
			twigs = (void*)ip;
			assert(Tindex_branch(i));
		} else {
			return(false);
		}
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
		Tbitmap b = 1U << nibble(i, *pkey, *plen);
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

static size_t
trunksize(Trie *t) {
	size_t s = 0;
	Tindex i = t->index;
	Trie *twigs = t->ptr;
	for(;;) {
		assert(Tindex_branch(i));
		uint max = popcount(Tindex_bitmap(i));
		s += max * sizeof(Trie);
		if(!hasconcat(i))
			return(s);
		Tindex *ip = (void*)(twigs + max);
		s += sizeof(Tindex);
		i = *ip++;
		twigs = (void*)ip;
	}
}

static void *
mdelete(void *vouter, size_t osize, void *vsplit, size_t dsize) {
	byte *outer = vouter, *split = vsplit;
	assert(outer <= split);
	assert(split < outer + osize);
	assert(split + dsize <= outer + osize);
	size_t suffix = (outer + osize) - (split + dsize);
	memmove(split, split + dsize, suffix);
	// If realloc() fails, continue to use the oversized allocation.
	void *maybe = realloc(outer, osize - dsize);
	return(maybe ? maybe : outer);
}

static void *
minsert(void *vouter, size_t osize, void *vsplit, void *vinsert, size_t isize) {
	byte *outer = vouter, *split = vsplit, *insert = vinsert;
	assert(outer <= split);
	assert(split < outer + osize);
	size_t prefix = split - outer;
	size_t suffix = (outer + osize) - split;
	outer = realloc(outer, osize + isize);
	if(outer == NULL) return(NULL);
	split = outer + prefix;
	memmove(split + isize, split, suffix);
	memmove(split, insert, isize);
	return(outer);
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(NULL);
	Trie *t = tbl, *p = NULL;
	Tindex i = 0;
	Tbitmap b = 0;
	while(isbranch(t)) {
		__builtin_prefetch(t->ptr);
		i = t->index;
		b = 1U << nibble(i, key, len);
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
	Trie *trunk = Tbranch_twigs(p);
	uint s = trunksize(p);
	assert(trunk <= t && t < trunk+s);
	uint m = popcount(Tindex_bitmap(i));
	if(m == 1) {
		// Our twig is the last in a rib branch, so we need
		// to remove the whole branch from the trunk. The
		// following index word, for the concatenated branch,
		// needs to be moved to the parent.
		*ip = *(Tindex *)(t + 1);
		Tset_twigs(p, mdelete(trunk, s, t,
				      sizeof(Trie) + sizeof(Tindex)));
		return(tbl);
	}
	if(m == 2) {
		// Move the other twig to the parent branch.
		*p = t[!s];
		free(t);
		return(tbl);
	}
	// Usual case
	*ip = Tbitmap_del(i, b);
	Tset_twigs(p, mdelete(trunk, s, t, sizeof(Trie)));
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
	while(isbranch(t)) {
		__builtin_prefetch(t->ptr);
		Tindex i = t->index;
		Tbitmap b = 1U << nibble(i, key, len);
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
	Tbitmap nb = 1U << knybble(key,off,shf);
	Tbitmap tb = 1U << knybble(tkey,off,shf);
	// Prepare the new leaf.
	Trie nt;
	Tset_key(&nt, key);
	Tset_val(&nt, val);
	// Find where to insert a branch or grow an existing branch.
	t = tbl;
	Tindex i = 0;
	while(isbranch(t)) {
		__builtin_prefetch(t->ptr);
		i = t->index;
		if(off == Tindex_offset(i) && shf == Tindex_shift(i))
			goto growbranch;
		if(off == Tindex_offset(i) && shf < Tindex_shift(i))
			goto newbranch;
		if(off < Tindex_offset(i))
			goto newbranch;
		Tbitmap b = 1U << nibble(i, key, len);
		assert(hastwig(i, b));
		t = Tbranch_twigs(t) + twigoff(i, b);
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	i = Tindex_new(shf, off, nb | tb);
	twigs[twigoff(i, nb)] = nt;
	twigs[twigoff(i, tb)] = *t;
	Tset_twigs(t, twigs);
	Tset_index(t, i);
	return(tbl);
growbranch:;
	assert(!hastwig(i, nb));
	uint s, m; TWIGOFFMAX(s, m, i, nb);
	twigs = realloc(Tbranch_twigs(t), sizeof(Trie) * (m + 1));
	if(twigs == NULL) return(NULL);
	memmove(twigs+s+1, twigs+s, sizeof(Trie) * (m - s));
	memmove(twigs+s, &nt, sizeof(Trie));
	Tset_twigs(t, twigs);
	Tset_index(t, Tbitmap_add(i, nb));
	return(tbl);
}
