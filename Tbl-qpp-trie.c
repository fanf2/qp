// Tbl-qpp-trie.c: tables implemented with quadbit popcount patricia tries.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

// In a trie, keys are divided into digits depending on some radix
// e.g. base 2 for binary tries, base 256 for byte-indexed tries.
// When searching the trie, successive digits in the key, from most to
// least significant, are used to select branches from successive
// nodes in the trie, like:
//	for(i = 0; isbranch(node); i++) node = node->branch[key[i]];
// All of the keys in a subtrie have identical prefixes. Tries do not
// need to store keys since they are implicit in the structure.
//
// A patricia trie or crit-bit trie is a binary trie which omits nodes that
// have only one child. Nodes are annotated with the index of the bit that
// is used to select the branch; indexes always increase as you go further
// into the trie. Each leaf has a copy of its key so that when you find a
// leaf you can verify that the untested bits match.
//
// Dan Bernstein has a nice description of crit-bit tries
//	http://cr.yp.to/critbit.html
// Adam Langley has annotated DJB's crit-bit implementation
//	https://github.com/agl/critbit
//
// You can use popcount() to implement a sparse array of length N
// containing M < N members using bitmap of length N and a packed
// vector of M elements. A member i is present in the array if bit
// i is set, so M == popcount(bitmap). The index of member i in
// the packed vector is the popcount of the bits preceding i.
//	mask = 1 << i;
//	if(bitmap & mask)
//		member = array[popcount(bitmap & mask-1)]
//
// Phil Bagwell's hashed array-mapped tries (HAMT) use popcount for
// compact trie nodes. String keys are hashed, and the hash is used
// as the index to the trie, with radix 2^32 or 2^64.
// http://infoscience.epfl.ch/record/64394/files/triesearches.pdf
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf
//
// A qpp trie uses its keys a quadbit (or nibble or half-byte) at a
// time. It is a radix 2^4 patricia trie, so each node can have between
// 2 and 16 children. It uses a 16 bit word to mark which children are
// present and popcount to index them. The aim is to improve on crit-bit
// tries by reducing memory usage and the number of indirections
// required to look up a key.
//
// The worst case for a qpp trie is when each branch has 2 children;
// then it is the same shape as a crit-bit trie. In this case there
// are n-1 internal branch nodes of two words each, so it is equally
// efficient as a crit-bit trie. If the key space is denser then
// branches have more children but the same overhead, so the memory
// usage is less. For maximally dense tries the overhead is:
//
// key length (bytes)    n
// number of leaves      256^n
// crit-bit branches     256^n - 1
// qpp branches          1 + 16^(n*2-1) == 1 + 256^n / 16

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

static inline uint
popcount(uint word) {
	return((uint)__builtin_popcount(word));
}

// A trie node is two words on 64 bit machines, or three on 32 bit
// machines. A node can be a leaf or a branch.

typedef struct Tleaf {
	const char *key;
	void *val;
} Tleaf;

// Branch nodes are distinguished from leaf nodes using a couple
// of flag bits which act as a dynamic type tag. They can be:
//
// 0 -> node is a leaf
// 1 -> node is a branch, testing upper nibble
// 2 -> node is a branch, testing lower nibble
//
// A branch node is laid out so that the flag bits correspond to the
// least significant bits bits of one of the leaf node pointers. In a
// leaf node, that pointer must be word-aligned so that its flag bits
// are zero. We have chosen to place this restriction on the value
// pointer.
//
// A branch contains the index of the byte that it tests. The combined
// value index << 2 | flags increases along the key in big-endian
// lexicographic order, and increases as you go deeper into the trie.
// All the keys below a branch are identical up to the nibble
// identified by the branch.
//
// A branch has a bitmap of which subtries ("twigs") are present. The
// flags, index, and bitmap are packed into one word. The other word
// is a pointer to an array of trie nodes, one for each twig that is
// present.

// XXX this currently assumes a 64 bit little endian machine
typedef struct Tbranch {
	union Trie *twigs;
	uint64_t
		flags : 2,
		index : 46,
		bitmap : 16;
} Tbranch;

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

// Make a bitmask for testing a branch bitmap.
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

// Extract a nibble from a key and turn it into a bitmask.

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
