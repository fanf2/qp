// Tbl-cri-bit.h: tables implemented with crit-bit tries.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

typedef unsigned char byte;
typedef unsigned int uint;

typedef struct Tleaf {
	const char *key;
	void *val;
} Tleaf;

// XXX this currently assumes a 64 bit little endian machine
typedef struct Tbranch {
	union Trie *twigs;
	uint64_t
		isbranch : 1,
		index : 63;
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
	return(t->branch.isbranch);
}

static inline uint
twigoff(Trie *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	if(i/8 >= len) return(0);
	return(key[i/8] >> (7 - i%8) & 1);
}

static inline Trie *
twig(Trie *t, uint i) {
	return(&t->branch.twigs[i]);
}
