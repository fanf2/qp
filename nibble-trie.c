// A combination of DJB's crit-bit trees (http://cr.yp.to/critbit.html)
// and Phil Bagwell's (hashed) array-mapped tries
// http://infoscience.epfl.ch/record/64394/files/triesearches.pdf
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf
// Something like this structure has been called a "poptrie"
// http://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p57.pdf

#include <assert.h>
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
isleaf(Tnode *t) {
	return(t->branch.flags == 0);
}

// Extract a nibble from a key.
//
// mask:
// 1 -> 0xffff -> 0xfff0 -> 0xf0
// 2 -> 0x0000 -> 0x000f -> 0x0f
//
// shift:
// 1 -> 1 -> 4
// 2 -> 0 -> 0

static inline unsigned
nibble(Tnode *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	if(i > len) return(0);
	unsigned flags = t->branch.flags;
	unsigned mask = ((flags - 2) ^ 0x0f) & 0xff;
	unsigned shift = (2 - flags) << 2;
	const byte *k = (const void *)key;
	return((k[i] & mask) >> shift);
}

static inline Tnode *
twig(Tnode *t, int i) {
	return(&t->branch.twigs[i]);
}

static inline bool
hastwig(Tnode *t, unsigned n) {
	unsigned m = 1 << n;
	return(t->branch.bitmap & m);
}

static inline int
twigcount(Tnode *t, unsigned n) {
	unsigned m = 1 << n;
	return(__builtin_popcount(t->branch.bitmap & (m - 1)));
}

void *
Tget(Tree *tree, const char *key) {
	if(tree == NULL)
		return(NULL);
	Tnode *t = &tree->root;
	size_t len = strlen(key);
	for(;;) {
		if(isleaf(t)) {
			if(strcmp(key, t->leaf.key) == 0)
				return(t->leaf.val);
			else
				return(NULL);
		}
		unsigned n = nibble(t, key, len);
		if(!hastwig(t, n))
			return(NULL);
		t = twig(t, twigcount(t, n));
	}
}

static const char *
next_rec(Tnode *t, const char *key, size_t len) {
	if(isleaf(t)) {
		if(key == NULL ||
		   strcmp(key, t->leaf.key) < 0)
			return(t->leaf.key);
		else
			return(NULL);
	}
	unsigned n = nibble(t, key, len);
	// This loop normally returns immediately, except when our key is
	// the last in its twig, in which case the loop tries the next
	// twig. Or if the key's twig is missing we run zero times.
        for(int i = twigcount(t, n), j = twigcount(t, 16); i < j; i++) {
		const char *found = next_rec(twig(t, i), key, len);
		if(found) return(found);
	}
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

Tree *
Tset(Tree *tree, const char *key, void *val) {
	// Ensure flag bits are zero.
	if(val != NULL)
		assert(((uint64_t)val & 3) == 0);
	// First leaf in an empty tree?
	if(tree == NULL) {
		if(val != NULL)
			tree = malloc(sizeof(*tree));
		if(tree != NULL) {
			tree->root.leaf.key = key;
			tree->root.leaf.val = val;
		}
		return(tree);
	}
	// delete is not yet implemented
	assert(val != NULL);
	Tnode *t = &tree->root;
	size_t len = strlen(key);
	// Find the most similar leaf node in the tree. We will compare
	// its key with our new key to find the first differing nibble,
	// which can be at a lower index than the point at which we
	// detect a difference.
	for(;;) {
		if(isleaf(t))
			break;
		unsigned n = nibble(t, key, len);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigcount(t, n) can cause
		// an out-of-bounds index if it equals twigcount(t, 16).
		int i = hastwig(t, n) ? twigcount(t, n) : 0;
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
	unsigned f = (byte)key[i] ^ (byte)t->leaf.key[i];
	f = (f & 0xf0) ? 1 : 2;
	// Find where to insert a node or replace an existing node.
	t = &tree->root;
	// This can only happen if there is one element in the tree.
	if(isleaf(t)) {
		Tnode *twigs = malloc(sizeof(Tnode) * 2);
		if(twigs == NULL) return(NULL);
		Tnode t1 = { .leaf.key = key, .leaf.val = val };
		Tnode t2 = *t;
		t->branch.twigs = twigs;
		t->branch.flags = f;
		t->branch.index = i;
		unsigned n1 = nibble(t, t1.leaf.key, i);
		unsigned n2 = nibble(t, t2.leaf.key, i);
		t->branch.bitmap = 1 << n1 | 1 << n2;
		*twig(t, twigcount(t, n1)) = t1;
		*twig(t, twigcount(t, n2)) = t2;
		return(tree);
	}
}
