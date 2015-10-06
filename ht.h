// ht.h: tables implemented with hash array mapped tries
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

typedef unsigned char byte;
typedef unsigned int uint;

// Word size parameters.
//   lgN: number of bits in a word
// lglgN: number of bits to index a word
//
// Branch maps are lgN bits wide (same as a pointer)
// Hash values are consumed lglgN bits at a time
//
// Hash values are always 64 bits (defined by SipHash)

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF

#define   lgN 64
#define lglgN 6

static inline uint
popcount(uintptr_t w) {
	return((uint)__builtin_popcountll(w));
}

#endif
#if UINTPTR_MAX == 0xFFFFFFFF

#define   lgN 32
#define lglgN 5

static inline uint
popcount(uintptr_t w) {
	return((uint)__builtin_popcount(w));
}

#endif

#define Hbits 64

extern int
siphash(uint8_t *out, const uint8_t *in, uint64_t inlen, const uint8_t *k);


typedef struct Tleaf {
	const char *key;
	void *val;
} Tleaf;

typedef struct Tbranch {
	uintptr_t map;
	uintptr_t twigs;
} Tbranch;

typedef union Trie {
	struct Tleaf   leaf;
	struct Tbranch branch;
} Trie;

struct Tbl {
	union Trie root;
};

static inline bool
isbranch(Trie *t) {
	return(t->branch.twigs & 1);
}

static inline uintptr_t
twigbit(uint64_t h) {
	return((uintptr_t)1 << (h & (lgN-1)));
}

static inline bool
hastwig(Trie *t, uintptr_t bit) {
	return(t->branch.map & bit);
}

static inline uint
twigoff(Trie *t, uintptr_t bit) {
	return(popcount(t->branch.map & (bit - 1)));
}

static inline uint
twigmax(Trie *t) {
	return(popcount(t->branch.map));
}

static inline Trie *
twig(Trie *t, uint i) {
	return((Trie*)(t->branch.twigs ^ 1) + i);
}

static inline void
twigset(Trie *t, Trie *twigs) {
	t->branch.twigs = (uintptr_t)twigs | 1;
}
