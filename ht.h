// ht.h: tables implemented with hash array mapped tries
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

typedef unsigned char byte;
typedef unsigned int uint;

static inline uint
popcount(uint64_t w) {
	return((uint)__builtin_popcountll(w));
}

extern int
siphash(uint8_t *out, const uint8_t *in, uint64_t inlen, const uint8_t *k);

#define Trie Tbl
struct Trie {
	union {
		const char *key;
		uint64_t map;
	};
	union {
		void *val;
		uintptr_t twigs;
	};
};

static inline bool
isbranch(Trie *t) {
	return(t->twigs & 1);
}

static inline uint64_t
twigbit(uint64_t k) {
	return(1ULL << (k & 077));
}

static inline bool
hastwig(Trie *t, uint64_t bit) {
	return(t->map & bit);
}

static inline uint
twigoff(Trie *t, uint64_t bit) {
	return(popcount(t->map & (bit - 1)));
}

static inline uint
twigmax(Trie *t) {
	return(popcount(t->map));
}

static inline Trie *
twig(Trie *t, uint i) {
	return((Trie*)(t->twigs ^ 1) + i);
}

static inline void
twigset(Trie *t, Trie *twigs) {
	t->twigs = (uintptr_t)twigs | 1;
}
