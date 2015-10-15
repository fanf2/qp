// wp.h: word-wide popcount patricia tries
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

// See qp.h for introductory comments about tries.
//
// The wp trie code is a straightforward clone-and-hack of the qp trie
// code. The difference is that the key is used 6 bits at a time
// instead of 4 bits, so the bitmap is 2^6 == 64 bits wide instead of
// 2^4 == 16 bits wide. Trie nodes are three words instead of two words.
//
// These bigger nodes mean that (currently) space is wasted in the
// leaf nodes. It might be possible to reclaim this space by embedding
// (short) keys in the leaves - see notes-embed-key.md

typedef unsigned char byte;
typedef unsigned int uint;

typedef uint64_t Tbitmap;

const char *dump_bitmap(Tbitmap w);

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
// Key indexes and shifts are numbered big-endian, so that they
// increase as we go along the key from left to right.
//
// 6-bit chunks never overlap, so they always have a fixed alignment
// relative to groups of three bytes, as illustrated below. We only
// need to care about this alignment when we are working out the
// position of the critical 6-bit chunk of a new key. At other times
// what matters is that a 6-bit chunk occupies part of at most two
// bytes, so the shift tells us how to pull the relevant bits out of
// those two bytes.
//
//  ..key[i%3==0].. ..key[i%3==1].. ..key[i%3==2]..
// |               |               |               | bytes
//  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
// |           |           |           |           | 6bits
//  ..shift=0.. ..shift=6.. ..shift=4.. ..shift=2..

static inline Tbitmap
nibbit(uint k, uint flags) {
	uint shift = 16 - 6 - (flags & 6);
	return(1ULL << ((k >> shift) & 0xFFFULL));
}

// Extract a nibble from a key and turn it into a bitmask.

static inline Tbitmap
twigbit(Trie *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	if(i >= len) return(1ULL);
	uint k = (byte)key[i] << 8;
	if(i+1 < len)
		k |= (byte)key[i+1];
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
