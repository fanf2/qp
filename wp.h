// wp.h: word-wide popcount patricia tries
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

typedef unsigned char byte;
typedef unsigned int uint;

typedef uint64_t Tbitmap;

#if defined(HAVE_SLOW_POPCOUNT)

static inline uint
popcount(Tbitmap w) {
	const uint64_t m1 = 0x5555555555555555;
	const uint64_t m2 = 0x3333333333333333;
	const uint64_t m4 = 0x0F0F0F0F0F0F0F0F;
	const uint64_t m7 = 0x0101010101010101;
	w -= (w >> 1) & m1;
	w = (w & m2) + ((w >> 2) & m2);
	w = (w + (w >> 4)) & m4;
	w = (w * m7) >> 56;
	return(w);
}

#else

static inline uint
popcount(Tbitmap w) {
	return((uint)__builtin_popcountll(w));
}

#endif

typedef struct Tleaf {
	const char *key;
	void *val;
	uint64_t wasted;
} Tleaf;

// flags & 1 == isbranch
// flags & 6 == shift

typedef struct Tbranch {
	union Trie *twigs;
	Tbitmap bitmap;
	uint64_t flags : 3,
	         index : 61;
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
	return(t->branch.flags & 1);
}

// We need to extract 6 bits from the key, 2^6 == 64
//
// Diagram of possible alignments of 6 bits relative to bytes.
// Bits are numbered little-endian from 0, like in a register.
// Shifts are numbered big-endian like key indexes.
//
//  ....key[0]..... ....key[1].....
//  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
// |           |                      index=0 shift=0
//     |           |                  index=0 shift=2
//         |           |              index=0 shift=4
//             |           |          index=0 shift=6
//                 |           |      index=1 shift=0
//                     |           |  index=1 shift=2
//
// BUT NOTE! We never use overlapping 6-bit sections as suggested
// by that diagram, we use successive sections like:
//
// 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2  index % 3
// 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0  bit number
// 0 0 0 0 0 0 6 6 6 6 6 6 4 4 4 4 4 4 2 2 2 2 2 2  shift

static inline Tbitmap
nibbit(uint k, uint flags) {
	uint shift = 16 - 6 - (flags & 6);
	return(1ULL << ((k >> shift) & 0xFFFULL));
}

// Extract a nibble from a key and turn it into a bitmask.

static inline Tbitmap
twigbit(Trie *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	uint k = 0;
	if(i < len) k |= (byte)key[i] << 8;
	if(i+1 < len) k |= (byte)key[i+1];
	return(nibbit(k, t->branch.flags));
}

static inline bool
hastwig(Trie *t, Tbitmap bit) {
	return(t->branch.bitmap & bit);
}

static inline uint
twigoff(Trie *t, Tbitmap b) {
	return(popcount(t->branch.bitmap & (b-1ULL)));
}

static inline Trie *
twig(Trie *t, uint i) {
	return(&t->branch.twigs[i]);
}

#define TWIGOFFMAX(off, max, t, b) do {			\
		off = twigoff(t, b);			\
		max = popcount(t->branch.bitmap);	\
	} while(0)
