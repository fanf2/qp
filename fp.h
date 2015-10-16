// fp.h: tables implemented with fivebit popcount patricia tries.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

typedef unsigned char byte;
typedef unsigned int uint;

typedef uint32_t Tbitmap;

const char *dump_bitmap(Tbitmap w);

#if defined(HAVE_SLOW_POPCOUNT)

static inline uint
popcount(Tbitmap w) {
	w -= (w >> 1) & 0x55555555;
	w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	w = (w + (w >> 4)) & 0x0F0F0F0F;
	w = (w * 0x01010101) >> 24;
	return(w);
}

#else

static inline uint
popcount(Tbitmap w) {
	return((uint)__builtin_popcount(w));
}

#endif

typedef struct Tleaf {
	const char *key;
	void *val;
} Tleaf;

typedef struct Tbranch {
	union Trie *twigs;
	uint64_t flags : 4,
		 index : 28,
		 bitmap : 32;
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

//  ..key[i%5==0].. ..key[i%5==1].. ..key[i%5==2].. ..key[i%5==3].. ..key[i%5==4]..
// |               |               |               |               |               |
//  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
// |         |         |         |         |         |         |         |         |
//  shift=0   shift=5   shift=2   shift=7   shift=4   shift=1   shift=6   shift=3

static inline Tbitmap
nibbit(uint k, uint flags) {
	uint shift = 16 - 5 - (flags >> 1);
	return(1U << ((k >> shift) & 0x1FU));
}

// Extract a nibble from a key and turn it into a bitmask.

static inline Tbitmap
twigbit(Trie *t, const char *key, size_t len) {
	uint64_t i = t->branch.index;
	if(i >= len) return(1);
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
	return(popcount(t->branch.bitmap & (b-1)));
}

static inline Trie *
twig(Trie *t, uint i) {
	return(&t->branch.twigs[i]);
}

#define TWIGOFFMAX(off, max, t, b) do {			\
		off = twigoff(t, b);			\
		max = popcount(t->branch.bitmap);	\
	} while(0)
