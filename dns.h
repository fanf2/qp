// dns-trie.c: a qp trie tuned for domain names
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

// WHAT IS A TRIE?
//
// A trie is another name for a radix tree, short for reTRIEval. In a
// trie, keys are divided into digits depending on some radix e.g. base
// 2 for binary tries, base 256 for byte-indexed tries. When searching
// the trie, successive digits in the key, from most to least
// significant, are used to select branches from successive nodes in the
// trie, like:
//
//	for(off = 0; isbranch(node); off++)
//		node = node->branch[key[off]];
//
// All of the keys in a subtrie have identical prefixes. Tries do not
// need to store keys since they are implicit in the structure.
//
// A patricia trie or crit-bit trie is a binary trie which omits nodes that
// have only one child. Nodes are annotated with the offset of the bit that
// is used to select the branch; offsets always increase as you go deeper
// into the trie. Each leaf has a copy of its key so that when you find a
// leaf you can verify that the untested bits match.
//
// SPARSE VECTORS WITH POPCOUNT
//
// The popcount() function counts the number of bits that are set in
// a word. It's also known as the Hamming weight; Knuth calls it
// "sideways add". https://en.wikipedia.org/wiki/popcount
//
// You can use popcount() to implement a sparse vector of length N
// containing M <= N members using bitmap of length N and a packed
// vector of M elements. A member b is present in the vector if bit
// b is set, so M == popcount(bitmap). The index of member b in
// the packed vector is the popcount of the bits preceding b.
//
//	mask = 1 << b;
//	if(bitmap & mask)
//		member = vector[popcount(bitmap & mask-1)]
//
// See "Hacker's Delight" by Hank Warren, section 5-1 "Counting 1
// bits", subsection "applications". http://www.hackersdelight.org
//
// POPCOUNT FOR TRIE NODES
//
// Phil Bagwell's hashed array-mapped tries (HAMT) use popcount for
// compact trie nodes. String keys are hashed, and the hash is used
// as the index to the trie, with radix 2^32 or 2^64.
// http://infoscience.epfl.ch/record/64394/files/triesearches.pdf
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf
//
// The performance of of a trie depends on its depth. A larger radix
// correspondingly reduces the depth, so it should be faster. The
// downside is usually much greater memory overhead.
//
// QP TRIE
//
// A qp-trie is a radix 16 or 32 patricia trie, so it uses its keys 4 or
// 5 bits at a time. It uses 16-wide or 32-wide bitmap to mark which
// children are present and popcount to index them. It is faster than a
// crit-bit trie, and uses less memory because it requires fewer nodes
// and popcount compresses them very efficiently.
//
// The fan-out of a qp-trie is limited by the size of a word; 16 or 32
// works well, and 32 is slightly faster. But radix 64 requires an extra
// word per node, and the extra memory overhead makes it slower.
//
// DNS TRIE
//
// A DNS-trie is a variant of a qp-trie tuned for keys that use the
// usual hostname alphabet of (case-insensitive) letters, digits,
// hyphen, plus underscore (which is often used for non-hostname
// purposes), and finally the label separator (which is written as '.'
// in presentation-format domain names).
//
// When a key only uses these characters, a DNS-trie is equivalent to a
// byte-at-a-time radix 256 trie. But it doesn't use any more memory
// than a qp-trie, because a 39-wide bitmap still fits in a word.
//
// To support keys that use unusual characters, a DNS-trie can use 2
// nodes per byte, like a 4-bit qp trie, except that the split is 3+5
// bits instead of 4+4 bits. The 8-wide bitmap for the upper three
// bits is added to the 39-wide bitmap for common characters, for a
// total of 47 bits. The 32-wide bitmap for the lower 5 bits is the
// same size as in a 5-bit qp-trie.
//
// The hostname characters are interspersed within the blocks of 32
// non-hostname characters. The most fragmented block is from 32-63,
// which is broken into 32-44, hyphen, 46, 47, digits, 58-63. This
// fragmentation makes it awkward to iterate over the trie in lexical
// order. So that we don't have to switch back and forth between parent
// and child nodes, the upper 3 bits of a non-hostname character are not
// used directly, but instead we assign a bit in the bitmap for each
// contiguous block of non-hostname characters, and each contiguous
// block is split on mod 32 boundaries.
//
// The index word also needs to contain an offset into the key, so the
// size of this offset field limits the maximum length of a key. Domain
// names have a maximum length of 255 bytes, so the large DNS-trie
// bitmap is not a problem.

////////////////////////////////////////////////////////////////////////
//                _       _
//   _ _  ___  __| |___  | |_ _  _ _ __  ___ ___
//  | ' \/ _ \/ _` / -_) |  _| || | '_ \/ -_|_-<
//  |_||_\___/\__,_\___|  \__|\_, | .__/\___/__/
//                            |__/|_|

typedef unsigned char byte;

// Type-punned words, that can be a 64-bit integer or a pointer.
//
// Normally the right type for this kind of word is uintptr_t, but our
// word needs to be at least 64 bits. However we can't just use uint64_t
// because that is not right when pointers are larger than 64 bits, e.g.
// on IBM AS/400 or Cambridge's CHERI capability architecture. So we use
// uint64_t on narrow machiness and uintptr_t everywhere else.
//
// It is easier to treat narrow architectures as a special case rather
// than to treat super-wide architectures as a special case because, for
// example, the arithmetic range of uintptr_t on CHERI is still 64 bits,
// and we can't use sizeof() in #if to check word sizes more directly.
//
// We aren't using a union to avoid problems with strict aliasing, and
// we aren't using bitfields because we want to choose the order of bits
// based on their significance regardless of the compiler's endianness.
//
#if UINTPTR_MAX < UINT32_MAX
#error pointers must be at least 32 bits
#elif UINTPTR_MAX < UINT64_MAX
typedef uint64_t word;
#else
typedef uintptr_t word;
#endif

#define W1 ((word)1)

// Type of the number of bits set in a word (as in Hamming Weight or
// popcount) which is used as the position of a node in the sparse
// vector of twigs.
//
typedef byte Weight;

// Type of the number of a bit inside a word (0..63).
//
typedef byte Shift;

// Type of a trie lookup key.
//
// A lookup key is an array of bit numbers. A domain name can be up to
// 255 bytes. When converted to a key, each byte in the name can be
// expanded to two bit numbers in the key when the byte isn't a common
// character. So we allow keys to be up to 512 bytes. (The actual max is
// a few smaller.)
//
#define KEY_SIZE_LOG2 9
typedef Shift Key[1 << KEY_SIZE_LOG2];

// A trie node is a pair of words, which can be a leaf or a branch.
//
// In a branch:
//
// `ptr` is a pointer to the "twigs", a sparse vector of child nodes.
//
// `index` contains flags, bitmap, and offset, describing the twigs.
// The bottom bits are non-zero.
//
// In a leaf:
//
// `ptr` points to a domain name in wire format.
//
// `index` is cast from a void* value, which must be word aligned
// so that the bottom bits are zero.
//
typedef struct Node {
	void *ptr;
	word index;
} Node;

// The root of a trie is a solitary node.
//
struct Tbl {
	Node root;
};

////////////////////////////////////////////////////////////////////////
//   _         _                             _
//  (_)_ _  __| |_____ __ __ __ _____ _ _ __| |
//  | | ' \/ _` / -_) \ / \ V  V / _ \ '_/ _` |
//  |_|_||_\__,_\___/_\_\  \_/\_/\___/_| \__,_|
//

// Index word layout.
//
// This enum sets up the bit positions of the parts of the index word.
//
// When an index word contains a pointer it must be word-aligned so that
// the tag and mark bits are zero.
//
// The bitmap is placed above the tag bits. The bit tests are set up to
// work directly against the index word; we don't need to extract the
// bitmap before testing a bit, but we do need to mask the bitmap before
// calling popcount.
//
// Each block in the bitmap corresponds to up to 32 non-hostname
// character values, which can hang off a child node.
//
// The key byte offset is at the top of the word, so that it can be
// extracted with just a shift, with no masking needed.
//
enum {
	// flags
	SHIFT_COW,		// copy-on-write marker
	SHIFT_BRANCH,		// branch / leaf tag
	// bitmap
	SHIFT_NOBYTE,		// label separator has no byte value
	SHIFT_0,		// block 0, control characters
	SHIFTa1,		// block 1, before hyphen
	SHYPHEN,
	SHIFTb1,		// block 1, between hypen and zero
	SHIFT_DIGIT,
	TOP_DIGIT = SHIFT_DIGIT + '9' - '0',
	SHIFTc1,		// block 1, after nine
	SHIFT_2,		// block 2, excluding uppercase
	UNDERBAR,
	BACKQUO,		// block 3, backquote
	SHIFT_LETTER,
	TOP_LETTER = SHIFT_LETTER + 'z' - 'a',
	SHIFT_3,		// block 3, after 'z'
	SHIFT_4,		// non-ascii blocks
	SHIFT_5,
	SHIFT_6,
	SHIFT_7,
	// key byte
	SHIFT_OFFSET,
	TOP_OFFSET = SHIFT_OFFSET + KEY_SIZE_LOG2,
};

typedef char static_assert_bitmap_fits_in_word
	[TOP_OFFSET < 64 ? 1 : -1];

// The mask covering just the flag bits.
//
#define MASK_FLAGS ((W1 << SHIFT_COW) | (W1 << SHIFT_BRANCH))

// The mask covering the bitmap part of an index word. The offset is directly
// after the bitmap so this mask consists of the offset's lesser bits.
//
#define MASK_BITMAP bit_to_mask(SHIFT_OFFSET)

// The mask covering the bits in a bitmap corresponding to split bytes.
//
#define MASK_SPLIT ( (W1 << SHIFT_0) |		\
		     (W1 << SHIFTa1) |		\
		     (W1 << SHIFTb1) |		\
		     (W1 << SHIFTc1) |		\
		     (W1 << SHIFT_2) |		\
		     (W1 << SHIFT_3) |		\
		     (W1 << SHIFT_4) |		\
		     (W1 << SHIFT_5) |		\
		     (W1 << SHIFT_6) |		\
		     (W1 << SHIFT_7) )

static inline bool
byte_is_split(Shift bit) {
	return(MASK_SPLIT & W1 << bit);
}

// Position of bitmap in the index word of a node for the less significant
// 5 bits of a split byte. Aligned so that NOBYTE is never set when there
// is a byte value.
//
#define SHIFT_LOWER SHIFT_0

typedef char static_assert_lower_bitmap_fits_in_upper_bitmap
	[SHIFT_LOWER + 32 < SHIFT_OFFSET ? 1 : -1];

// Bit position for lower 5 bits of a split byte.
//
static inline Shift
lower_to_bit(byte b) {
	return(SHIFT_LOWER + b % 32);
}

// Given a bit number in the bitmap, return a mask covering the lesser
// bits in the bitmap.
//
static inline word
bit_to_mask(Shift bit) {
	return((W1 << bit) - 1 - MASK_FLAGS);
}

////////////////////////////////////////////////////////////////////////
//   _                     _
//  | |__ _ _ __ _ _ _  __| |_  ___ ___
//  | '_ \ '_/ _` | ' \/ _| ' \/ -_|_-<
//  |_.__/_| \__,_|_||_\__|_||_\___/__/
//

static inline bool
isbranch(Node *n) {
	return(n->index & W1 << SHIFT_BRANCH);
}

static inline size_t
keyoff(Node *n) {
	return(n->index >> SHIFT_OFFSET);
}

static inline Shift
twigbit(Node *n, const Key key, size_t len) {
	word off = keyoff(n);
	if(off < len) return(key[off]);
	else return(SHIFT_NOBYTE);
}

static inline bool
hastwig(Node *n, Shift bit) {
	return(n->index & W1 << bit);
}

static inline Weight
bmpcount(Node *n, word mask) {
	unsigned long long bmp = (unsigned long long)(n->index & mask);
	return((Weight)__builtin_popcountll(bmp));
}

static inline Weight
twigmax(Node *n) {
	return(bmpcount(n, MASK_BITMAP));
}

static inline Weight
twigoff(Node *n, Shift bit) {
	return(bmpcount(n, bit_to_mask(bit)));
}

// This is used when we need to keep iterating down to a leaf even if
// our key is missing from this branch. It doesn't matter which twig we
// choose since the keys are all the same up to this node's offset. Note
// that blindly using twigoff(n, bit) can cause an out-of-bounds access
// if our bit is greater than all the set bits in the node.
static inline Weight
neartwig(Node *n, Shift bit) {
	return(hastwig(n, bit) ? twigoff(n, bit) : 0);
}

static inline Node *
twig(Node *n, Weight i) {
	return((Node *)n->ptr + i);
}

////////////////////////////////////////////////////////////////////////
