// cb.h: tables implemented with crit-bit tries.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

// See qp.h for introductory comments on tries.
//
// Dan Bernstein has a well-known description of crit-bit tries
//	http://cr.yp.to/critbit.html
// Adam Langley has annotated DJB's crit-bit implementation
//	https://github.com/agl/critbit
//
// DJB's crit-bit tries only store a set of keys, without any
// associated values. The branch nodes have three words: a bit index,
// and two pointers to child nodes. Each child pointer has a flag in
// its least significant bit indicating whether it points to another
// branch, or points to a key string.
//
// [ ptr B ] -> [ index ]
//              [ ptr L ] -> "leaf 0"
//              [ ptr B ]
//                       \
//                        +-> [ index ]
//                            [ ptr L ] -> "leaf 1"
//                            [ ptr L ] -> "leaf 2"
//
// An important property of these tries is their low overhead, two
// words per entry in addition to the key pointer itself. It is hard
// to add associated values without increasing this overhead. If you
// simply replace each string pointer with a pointer to a key+value
// pair, the overhead is 50% greater: three words per entry in
// addition to the key+value pointers.
//
// This crit-bit implementation uses a different layout. A branch node
// contains a bit index and only one pointer. Its two children (called
// "twigs") are allocated as a pair; the bit in the key selects which
// twig in the pair is selected when traversing the trie. Now branch
// nodes are two words, the same size as a key+value leaf node, so any
// combination of leaves and branches packs nicely into a four-word
// pair of twigs. The flag bit is put in the node (e.g. least
// significant bit of the index is always set) rather than packing two
// flag bits into the twigs pointer.
//
// [ index 1 twig ] -> [ value 0 key  ] -> "leaf 0"
//                     [ index 1 twig ]
//                                     \
//                                      +-> [ value 0 key ] -> "leaf 1"
//                                          [ value 0 key ] -> "leaf 2"
//
// Another way of looking at this is we have added two words to each
// node for the value pointers; these are empty in branch nodes, which
// gives us space to move the bit indexes up a level, one bit index
// from each child to occupy each empty word. Moving the bit indexes
// takes away a word from every node, except for the root which
// becomes a word bigger.
//
// This layout has two words of overhead per entry, in addition to the
// key+value pointers.
//
// I originally developed this layout for qp tries, then simplified
// the qp code to produce this crit-bit implementation.

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
