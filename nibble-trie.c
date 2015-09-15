// A combination of DJB's crit-bit trees (http://cr.yp.to/critbit.html)
// and Phil Bagwell's (hashed) array-mapped tries
// http://infoscience.epfl.ch/record/64394/files/triesearches.pdf
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf

#include <stdint.h>
#include <string.h>

#include "nibble-trie.h"

#define popcount __builtin_popcount

typedef unsigned char byte;

// XXX Assume a little-endian struct layout, so that the flag bits
// fall in the clear bits at the bottom of the pointer. This needs
// to change on a big-endian C implementation.
//
// We arrange for the flag bits to match up with the value pointer
// because it is word-aligned and the key pointer can be byte-aligned.
//
// Flags can be
// 0 -> node is a leaf
// 1 -> node is a branch, testing upper nibble
// 2 -> node is a branch, testing lower nibble
//
// The combined value (index << 2) | flags increases in big-endian
// lexicographic order.

typedef struct Tbranch {
	union Tnode *nodes;
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
	union Tnode tree;
};

// Extract a nibble from a key.
//
// mask:
// 1 -> 0xffff -> 0xfff0 -> 0xf0
// 2 -> 0x0000 -> 0x000f -> 0x0f
//
// shift:
// 1 -> 1 -> 4
// 2 -> 0 -> 0
//
static inline unsigned nibble(const char *key, unsigned flags, uint64_t i) {
	unsigned mask = ((flags - 2) ^ 0x0f) & 0xff;
	unsigned shift = (2 - flags) << 2;
	const byte *k = (const byte *)key;
	return((k[i] & mask) >> shift);
}

void *Tget(Tree *tree, const char *key) {
	Tnode *t = &tree->tree;
	size_t len = strlen(key);

	for(;;) {
		if(t->branch.flags == 0) {
			if(strcmp(key, t->leaf.key) == 0)
				return(t->leaf.val);
			else
				return(NULL);
		}
		if(t->branch.index >= len)
			return(NULL);
		unsigned n = nibble(key, t->branch.flags, t->branch.index);
		unsigned m = 1 << n;
		if((t->branch.bitmap & m) == 0)
			return(NULL);
		unsigned i = popcount(t->branch.bitmap & (m - 1));
		t = t->branch.nodes + i;
	}
}

static const char *Trec(Tnode *t, const char *key, size_t len) {
	if(t->branch.flags == 0) {
		if(key == NULL ||
		   strcmp(key, t->leaf.key) < 0)
			return(t->leaf.key);
		else
			return(NULL);
	}
	unsigned n, m;
	if(t->branch.index >= len)
		n = 0;
	else
	        n = nibble(key, t->branch.flags, t->branch.index);
	m = (1 << n) - 1;
	unsigned i = popcount(t->branch.bitmap & m);
	unsigned j = popcount(t->branch.bitmap);
	while(i < j) {
		const char *found = Trec(t->branch.nodes + i, key, len);
		if(found) return(found);
		i++;
	}
	return(NULL);
}

const char *Tnext(Tree *tree, const char *key) {
	Tnode *t = &tree->tree;
	size_t len = key != NULL ? strlen(key) : 0;
	return(Trec(t, key, len));
}
