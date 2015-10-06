// qp.h: tables implemented with quadbit popcount patricia tries.
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
// The popcount() function counts the number of bits that are set in
// a word. It's also known as the Hamming weight; Knuth calls it
// "sideways add". https://en.wikipedia.org/wiki/popcount
//
// You can use popcount() to implement a sparse array of length N
// containing M < N members using bitmap of length N and a packed
// vector of M elements. A member i is present in the array if bit
// i is set, so M == popcount(bitmap). The index of member i in
// the packed vector is the popcount of the bits preceding i.
//	mask = 1 << i;
//	if(bitmap & mask)
//		member = vector[popcount(bitmap & mask-1)]
//
// See "AMD64 Optimization Guide", section 8-6 "Efficient Implementation
// of Population-Count Function in 32-Bit Mode"
// http://support.amd.com/TechDocs/25112.PDF
//
// Phil Bagwell's hashed array-mapped tries (HAMT) use popcount for
// compact trie nodes. String keys are hashed, and the hash is used
// as the index to the trie, with radix 2^32 or 2^64.
// http://infoscience.epfl.ch/record/64394/files/triesearches.pdf
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf
//
// A qp trie uses its keys a quadbit (or nibble or half-byte) at a
// time. It is a radix 2^4 patricia trie, so each node can have between
// 2 and 16 children. It uses a 16 bit word to mark which children are
// present and popcount to index them. The aim is to improve on crit-bit
// tries by reducing memory usage and the number of indirections
// required to look up a key.
//
// The worst case for a qp trie is when each branch has 2 children;
// then it is the same shape as a crit-bit trie. In this case there
// are n-1 internal branch nodes of two words each, so it is equally
// efficient as a crit-bit trie. If the key space is denser then
// branches have more children but the same overhead, so the memory
// usage is less. For maximally dense tries the overhead is:
//
// key length (bytes)    n
// number of leaves      256^n
// crit-bit branches     256^n - 1
// qp branches           1 + 16^(n*2-1) == 1 + 256^n / 16
// crit-bit depth        n * 8
// qp depth              n * 2
//
// In practice, qp averages about 3.3 words per leaf vs. crit-bit's 4
// words per leaf, and qp has about half the depth.

typedef unsigned char byte;
typedef unsigned int uint;

#ifndef HAVE_SLOW_POPCOUNT
static inline uint
popcount(uint w) {
	return((uint)__builtin_popcount(w));
}
#define popcount16(w) popcount(w)
#else
// 32 bit popcount() for use when __builtin_popcount() is slow.
static inline uint
popcount(uint w) {
	w -= (w >> 1) & 0x55555555;
	w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	w = (w + (w >> 4)) & 0x0f0f0f0f;
	w = (w * 0x01010101) >> 24;
	return(w);
}
// 16 bit popcount() for use when __builtin_popcount() is slow.
static inline uint
popcount16(uint w) {
	w -= (w >> 1) & 0x5555;
	w = (w & 0x3333) + ((w >> 2) & 0x3333);
	w = (w + (w >> 4)) & 0x0f0f;
	w = (w + (w >> 8)) & 0x00ff;
	return(w);
}
#endif

// A trie node is two words on 64 bit machines, or three on 32 bit
// machines. A node can be a leaf or a branch. In a leaf, the value
// pointer must be word-aligned to allow for the tag bits.

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

// XXX We hope that the compiler will not punish us for abusing unions.

// XXX This currently assumes a 64 bit little endian machine.
// On a 32 bit machine we could perhaps fit a branch in to two words
// without restricting the key length by making the index relative
// instead of absolute. If the gap between nodes is larger than a 16
// bit offset allows, we can insert a stepping-stone branch with only
// one twig. This would make the code a bit more complicated...

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
	if(i >= len) return(1);
	return(nibbit((byte)key[i], t->branch.flags));
}

static inline bool
hastwig(Trie *t, uint bit) {
	return(t->branch.bitmap & bit);
}

static inline uint
twigoff(Trie *t, uint bit) {
	return(popcount16(t->branch.bitmap & (bit - 1)));
}

#define TWIGOFFMAX(off, max, t, b) do {			\
		off = twigoff(t, b);			\
		max = popcount(t->branch.bitmap);	\
	} while(0)

// Argument i is result of twigoff()

static inline Trie *
twig(Trie *t, uint i) {
	return(&t->branch.twigs[i]);
}
