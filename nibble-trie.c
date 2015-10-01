// A combination of DJB's crit-bit trees (http://cr.yp.to/critbit.html)
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

#include "nibble-trie.h"

typedef unsigned char byte;

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
	union Tnode *twigs;
	uint64_t
		flags : 2,
		index : 46,
		bitmap : 16;
} Tbranch;

typedef struct Tleaf {
	const char *key;
	void *val;
} Tleaf;

typedef union Tnode {
	struct Tleaf   leaf;
	struct Tbranch branch;
} Tnode;

struct Tree {
	union Tnode root;
};

// Test flags to determine type of this node.

static inline bool
isbranch(Tnode *t) {
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

static inline unsigned
nibbit(byte k, unsigned flags) {
	unsigned mask = ((flags - 2) ^ 0x0f) & 0xff;
	unsigned shift = (2 - flags) << 2;
	return(1 << ((k & mask) >> shift));
}

static inline unsigned
twigbit(Tnode *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	if(i > len) return(0);
	return(nibbit((byte)key[i], t->branch.flags));
}

static inline bool
hastwig(Tnode *t, unsigned bit) {
	return(t->branch.bitmap & bit);
}

static inline int
twigoff(Tnode *t, unsigned bit) {
	return(__builtin_popcount(t->branch.bitmap & (bit - 1)));
}

static inline int
twigmax(Tnode *t) {
	return(__builtin_popcount(t->branch.bitmap));
}

static inline Tnode *
twig(Tnode *t, int i) {
	return(&t->branch.twigs[i]);
}

void *
Tget(Tree *tree, const char *key) {
	if(tree == NULL)
		return(NULL);
	Tnode *t = &tree->root;
	size_t len = strlen(key);
	while(isbranch(t)) {
		unsigned b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(NULL);
		t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, t->leaf.key) == 0)
		return(t->leaf.val);
	else
		return(NULL);
}

static const char *
next_rec(Tnode *t, const char *key, size_t len) {
	if(isbranch(t)) {
		// This loop normally returns immediately, except when our
		// key is the last in its twig, in which case the loop tries
		// the next twig. Or if the key's twig is missing we run zero
		// times.
		unsigned b = twigbit(t, key, len);
		for(int i = twigoff(t, b), j = twigmax(t); i < j; i++) {
			const char *found = next_rec(twig(t, i), key, len);
			if(found) return(found);
		}
		return(NULL);
	}
	if(key == NULL ||
	    strcmp(key, t->leaf.key) < 0)
		return(t->leaf.key);
	else
		return(NULL);
}

const char *
Tnext(Tree *tree, const char *key) {
	if(tree == NULL)
		return(NULL);
	Tnode *t = &tree->root;
	size_t len = key != NULL ? strlen(key) : 0;
	return(next_rec(t, key, len));
}

static Tree *
Tdel(Tree *tree, const char *key) {
	Tnode *t = &tree->root, *p = NULL;
	size_t len = strlen(key);
	unsigned b;
	while(isbranch(t)) {
		b = twigbit(t, key, len);
		if(!hastwig(t, b))
			return(tree);
		p = t; t = twig(t, twigoff(t, b));
	}
	if(strcmp(key, t->leaf.key) != 0)
		return(tree);
	if(p == NULL) {
		free(tree);
		return(NULL);
	}
	if(twigmax(p) == 2) {
		// Move the other twig to the parent branch.
		t = p->branch.twigs;
		*p = *twig(p, !twigoff(t, b));
		free(t);
		return(tree);
	}
	int s = twigoff(t, b); // split
	int m = twigmax(p);
	Tnode *twigs = malloc(sizeof(Tnode) * (m - 1));
	if(twigs == NULL) return(NULL);
	memcpy(twigs, p->branch.twigs, sizeof(Tnode) * s);
	memcpy(twigs+s, p->branch.twigs+s+1, sizeof(Tnode) * (m - s - 1));
	free(p->branch.twigs);
	p->branch.twigs = twigs;
	p->branch.bitmap &= ~b;
	return(tree);
}

Tree *
Tset(Tree *tree, const char *key, void *val) {
	// Ensure flag bits are zero.
	if(((uint64_t)val & 3) != 0) {
		errno = EINVAL;
		return(NULL);
	}
	if(val == NULL)
		return(Tdel(tree, key));
	// First leaf in an empty tree?
	if(tree == NULL) {
		tree = malloc(sizeof(*tree));
		if(tree == NULL) return(NULL);
		tree->root.leaf.key = key;
		tree->root.leaf.val = val;
		return(tree);
	}
	Tnode *t = &tree->root;
	size_t len = strlen(key);
	// Find the most similar leaf node in the tree. We will compare
	// its key with our new key to find the first differing nibble,
	// which can be at a lower index than the point at which we
	// detect a difference.
	while(isbranch(t)) {
		unsigned b = twigbit(t, key, len);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigcount(t, n) can cause
		// an out-of-bounds index if it equals twigcount(t, 16).
		int i = hastwig(t, b) ? twigoff(t, b) : 0;
		t = twig(t, i);
	}
	// Do the keys differ, and if so, where?
	size_t i;
	for(i = 0; i <= len; i++) {
		if(key[i] != t->leaf.key[i])
			goto newkey;
	}
	t->leaf.val = val;
	return(tree);
newkey:; // We have the branch's index; what are its flags?
	byte k1 = (byte)key[i], k2 = (byte)t->leaf.key[i];
	unsigned f =  k1 ^ k2;
	f = (f & 0xf0) ? 1 : 2;
	// Prepare the new leaf.
	unsigned b1 = nibbit(k1, f);
	Tnode t1 = { .leaf = { .key = key, .val = val } };
	// Find where to insert a branch or grow an existing branch.
	t = &tree->root;
	while(isbranch(t)) {
		if(i == t->branch.index && f == t->branch.flags)
			goto growbranch;
		if(i == t->branch.index && f < t->branch.flags)
			goto newbranch;
		if(i < t->branch.index)
			goto newbranch;
		unsigned b = twigbit(t, key, len);
		assert(hastwig(t, b));
		t = twig(t, twigoff(t, b));
	}
newbranch:;
	Tnode *twigs = malloc(sizeof(Tnode) * 2);
	if(twigs == NULL) return(NULL);
	Tnode t2 = *t; // Save before overwriting.
	unsigned b2 = nibbit(k2, f);
	t->branch.twigs = twigs;
	t->branch.flags = f;
	t->branch.index = i;
	t->branch.bitmap = b1 | b2;
	*twig(t, twigoff(t, b1)) = t1;
	*twig(t, twigoff(t, b2)) = t2;
	return(tree);
growbranch:;
	assert(!hastwig(t, b1));
	int s = twigoff(t, b1); // split
	int m = twigmax(t);
	twigs = malloc(sizeof(Tnode) * (m + 1));
	if(twigs == NULL) return(NULL);
	memcpy(twigs, t->branch.twigs, sizeof(Tnode) * s);
	memcpy(twigs+s, &t1, sizeof(Tnode));
	memcpy(twigs+s+1, t->branch.twigs+s, sizeof(Tnode) * (m - s));
	free(t->branch.twigs);
	t->branch.twigs = twigs;
	t->branch.bitmap |= b1;
	return(tree);
}
