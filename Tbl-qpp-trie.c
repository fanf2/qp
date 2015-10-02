// A combination of DJB's crit-bit tries (http://cr.yp.to/critbit.html)
// and Phil Bagwell's (hashed) array-mapped tries
// http://infoscience.epfl.ch/record/64394/files/triesearches.pdf
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf
// Something like this structure has been called a "poptrie"
// http://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p57.pdf

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Tbl.h"
#include "Tbl.c"

typedef unsigned char byte;
typedef unsigned int uint;

// XXX Assume a little-endian struct layout, so that the flag bits
// fall in the clear bits at the bottom of the pointer. This needs
// to change on a big-endian and/or 32 bit C implementation.
//
// The flags are a dynamic type tag. They can be:
// 0 -> node is a leaf
// 1 -> node is a branch, testing upper nibble
// 2 -> node is a branch, testing lower nibble
//
// In a branch, the combined value (index << 2) | flags
// increases along the key in big-endian lexicographic order.
// All the keys below a branch are identical up to the nibble
// identified by the branch.
//
// In a leaf node we arrange for the flag bits (which are zero)
// to match up with the value pointer which must therefore be
// word-aligned; the key pointer can be byte-aligned.

typedef struct Tbranch {
	union Trie *twigs;
	uint64_t
		flags : 2,
		index : 46,
		bitmap : 16;
} Tbranch;

typedef struct Tleaf {
	const char *key;
	void *val;
} Tleaf;

typedef union Trie {
	struct Tleaf   leaf;
	struct Tbranch branch;
} Trie;

struct Tbl {
	union Trie root;
};

// Test flags to determine type of this node.

static inline bool
isbranch(Trie *t) {
	return(t->branch.flags != 0);
}

// Extract a nibble from a key and make a bitmask for testing the bitmap.
//
// mask:
// 1 -> 0xffff -> 0xfff0 -> 0xf0
// 2 -> 0x0000 -> 0x000f -> 0x0f
//
// shift:
// 1 -> 1 -> 4
// 2 -> 0 -> 0

static inline uint
nibbit(byte k, uint flags) {
	uint mask = ((flags - 2) ^ 0x0f) & 0xff;
	uint shift = (2 - flags) << 2;
	return(1 << ((k & mask) >> shift));
}

static inline uint
twigbit(Trie *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	if(i > len) return(0);
	return(nibbit((byte)key[i], t->branch.flags));
}

static inline bool
hastwig(Trie *t, uint bit) {
	return(t->branch.bitmap & bit);
}

static inline uint
popcount(uint word) {
	return((uint)__builtin_popcount(word));
}

static inline uint
twigoff(Trie *t, uint bit) {
	return(popcount(t->branch.bitmap & (bit - 1)));
}

static inline uint
twigmax(Trie *t) {
	return(popcount(t->branch.bitmap));
}

static inline Trie *
twig(Trie *t, uint i) {
	return(&t->branch.twigs[i]);
}

void *
Tgetl(Tbl *tbl, const char *key, size_t len) {
	if(tbl == NULL)
		return(NULL);
	Trie *t = &tbl->root;
	while(isbranch(t)) {
		uint b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(NULL);
		t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, t->leaf.key) == 0)
		return(t->leaf.val);
	else
		return(NULL);
}

static bool
next_rec(Trie *t, const char **pkey, size_t *plen, void **pval) {
	if(isbranch(t)) {
		// This loop normally returns immediately, except when our
		// key is the last in its twig, in which case the loop tries
		// the next twig. Or if the key's twig is missing we run zero
		// times.
		uint b = twigbit(t, *pkey, *plen);
		for(uint i = twigoff(t, b), j = twigmax(t); i < j; i++)
			if(next_rec(twig(t, i), pkey, plen, pval))
				return(true);
		return(false);
	}
	if(*pkey == NULL ||
	   strcmp(*pkey, t->leaf.key) < 0) {
		*pkey = t->leaf.key;
		*plen = strlen(*pkey);
		*pval = t->leaf.val;
		return(true);
	}
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
Tdell(Tbl *tbl, const char *key, size_t len) {
	Trie *t = &tbl->root, *p = NULL;
	uint b = 0;
	while(isbranch(t)) {
		b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(tbl);
		p = t; t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, t->leaf.key) != 0)
		return(tbl);
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	if(twigmax(p) == 2) {
		// Move the other twig to the parent branch.
		t = p->branch.twigs;
		*p = *twig(p, !twigoff(t, b));
		free(t);
		return(tbl);
	}
	uint s = twigoff(t, b); // split
	uint m = twigmax(p);
	Trie *twigs = malloc(sizeof(Trie) * (m - 1));
	if(twigs == NULL) return(NULL);
	memcpy(twigs, p->branch.twigs, sizeof(Trie) * s);
	memcpy(twigs+s, p->branch.twigs+s+1, sizeof(Trie) * (m - s - 1));
	free(p->branch.twigs);
	p->branch.twigs = twigs;
	p->branch.bitmap &= ~b;
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
	while(isbranch(t)) {
		uint b = twigbit(t, key, len);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigcount(t, n) can cause
		// an out-of-bounds index if it equals twigcount(t, 16).
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
	uint b1 = nibbit(k1, f);
	Trie t1 = { .leaf = { .key = key, .val = val } };
	// Find where to insert a branch or grow an existing branch.
	t = &tbl->root;
	while(isbranch(t)) {
		if(i == t->branch.index && f == t->branch.flags)
			goto growbranch;
		if(i == t->branch.index && f < t->branch.flags)
			goto newbranch;
		if(i < t->branch.index)
			goto newbranch;
		uint b = twigbit(t, key, len);
		assert(hastwig(t, b));
		t = twig(t, twigoff(t, b));
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	Trie t2 = *t; // Save before overwriting.
	uint b2 = nibbit(k2, f);
	t->branch.twigs = twigs;
	t->branch.flags = f;
	t->branch.index = i;
	t->branch.bitmap = b1 | b2;
	*twig(t, twigoff(t, b1)) = t1;
	*twig(t, twigoff(t, b2)) = t2;
	return(tbl);
growbranch:;
	assert(!hastwig(t, b1));
	uint s = twigoff(t, b1); // split
	uint m = twigmax(t);
	twigs = malloc(sizeof(Trie) * (m + 1));
	if(twigs == NULL) return(NULL);
	memcpy(twigs, t->branch.twigs, sizeof(Trie) * s);
	memcpy(twigs+s, &t1, sizeof(Trie));
	memcpy(twigs+s+1, t->branch.twigs+s, sizeof(Trie) * (m - s));
	free(t->branch.twigs);
	t->branch.twigs = twigs;
	t->branch.bitmap |= b1;
	return(tbl);
}
