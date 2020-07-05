// dns-trie.c: a qp trie tuned for domain names
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Tbl.h"

typedef unsigned char byte;

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
// work well, and 32 is slightly faster. But radix 64 requires an extra
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
// To support keys that use unusual characters, a DNS-trie uses 2 nodes
// per byte, like a 4-bit qp trie, except that the split is 3+5 bits
// instead of 4+4 bits. The 8-wide bitmap for the upper node is added to
// the 39-wide bitmap for common characters, for a total of 47 bits. The
// 32-wide bitmap for the lower node is the same size as a 5-bit
// qp-trie.
//
// The hostname characters are interspersed with the blocks of 32
// non-hostname characters. The block from 32-63 is broken into 32-44,
// hyphen, 46, 47, digits, 58-63. This makes it awkward to iterate over
// the trie in lexical order. So that we don't have to switch back and
// forth between parent and child nodes, the upper 3 bits of a
// non-hostname character are not used directly, but instead we assign a
// bit in the bitmap for each contiguous block of non-hostname
// characters, and each contiguous block is split on 5 bit boundaries.
//
// The index word also needs to contain an offset into the key, so the
// size of this offset field limits the maximum length of a key. Domain
// names have a maximum length of 255 bytes, so the large DNS-trie
// bitmap is not a problem.

////////////////////////////////////////////////////////////////////////
//   _         _                             _
//  (_)_ _  __| |_____ __ __ __ _____ _ _ __| |
//  | | ' \/ _` / -_) \ / \ V  V / _ \ '_/ _` |
//  |_|_||_\__,_\___/_\_\  \_/\_/\___/_| \__,_|
//

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

// Index word layout.
//
// When an index word contains a pointer it must be word-aligned so that
// the tag and mark bits are zero.
//
// The bitmap is placed above the tag bits. The bit tests are set up to
// work directly against the index word; we don't need to extract the
// bitmap before testing a bit, but we do need to mask the bitmap before
// calling popcount.
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
	SHIFT_2,		// block 2, excluding letters
	UNDERBAR,
	BACKQUO,		// block 3, backquote
	SHIFT_LETTER,
	TOP_LETTER = SHIFT_LETTER + 'z' - 'a',
	SHIFT_3,		// block 3, curly - del
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

// The mask covering the bitmap part of an index word. The offset
// follows the bitmap so we want the offset's lesser bits.
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
	return((W1 << bit) - MASK_FLAGS - 1);
}

////////////////////////////////////////////////////////////////////////
//   _         _         _         _    _ _
//  | |__ _  _| |_ ___  | |_ ___  | |__(_) |_
//  | '_ \ || |  _/ -_) |  _/ _ \ | '_ \ |  _|
//  |_.__/\_, |\__\___|  \__\___/ |_.__/_|\__|
//        |__/

#define DD(byte) (SHIFT_DIGIT  + byte - '0')
#define LL(byte) (SHIFT_LETTER + byte - 'a')

// Map a byte of a key to a bit number in an index word.
/*

;; the table below is constructed by this elisp
(progn
  ; empty the table
  (search-forward "{")
  (set-mark (point))
  (search-forward "}")
  (kill-region (1+ (region-beginning))
	       (1- (region-end)))
  (beginning-of-line)
  ; insert a table entry for each possible byte value
  (dotimes (byte 256)
    ; indent
    (when (= 0 (% byte 8))
      (insert "\t"))
    (cond
      ; hyphen and digits and neighbours
      ((and (>= byte 32) (< byte ?-))
        (insert "SHIFTa1"))
      ((= byte ?-)
        (insert "SHYPHEN"))
      ((and (> byte ?-) (< byte ?0))
        (insert "SHIFTb1"))
      ((and (>= byte ?0) (<= byte ?9))
        (insert "DD('" (byte-to-string byte) "')"))
      ((and (> byte ?0) (< byte 64))
        (insert "SHIFTc1"))
      ; because upper-case letters don't sort here,
      ; block 2 is effectively contiguous
      ((and (>= byte ?A) (<= byte ?Z))
        (insert "LL('" (byte-to-string (+ 32 byte)) "')"))
      ((= byte ?_)
        (insert "UNDERBAR"))
      ; block 3
      ((= byte ?`)
        (insert "BACKQUO"))
      ((and (>= byte ?a) (<= byte ?z))
        (insert "LL('" (byte-to-string byte) "')"))
      ; simple blocks
      (t (insert "SHIFT_" (number-to-string (/ byte 32)))))
    ; separators
    (if (= 7 (% byte 8))
      (insert ",\n")
      (insert ", "))))

 */
static const Shift byte_to_bit[256] = {
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1,
	SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHYPHEN, SHIFTb1, SHIFTb1,
	DD('0'), DD('1'), DD('2'), DD('3'), DD('4'), DD('5'), DD('6'), DD('7'),
	DD('8'), DD('9'), SHIFTc1, SHIFTc1, SHIFTc1, SHIFTc1, SHIFTc1, SHIFTc1,
	SHIFT_2, LL('a'), LL('b'), LL('c'), LL('d'), LL('e'), LL('f'), LL('g'),
	LL('h'), LL('i'), LL('j'), LL('k'), LL('l'), LL('m'), LL('n'), LL('o'),
	LL('p'), LL('q'), LL('r'), LL('s'), LL('t'), LL('u'), LL('v'), LL('w'),
	LL('x'), LL('y'), LL('z'), SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, UNDERBAR,
	BACKQUO, LL('a'), LL('b'), LL('c'), LL('d'), LL('e'), LL('f'), LL('g'),
	LL('h'), LL('i'), LL('j'), LL('k'), LL('l'), LL('m'), LL('n'), LL('o'),
	LL('p'), LL('q'), LL('r'), LL('s'), LL('t'), LL('u'), LL('v'), LL('w'),
	LL('x'), LL('y'), LL('z'), SHIFT_3, SHIFT_3, SHIFT_3, SHIFT_3, SHIFT_3,
	SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4,
	SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4,
	SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4,
	SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4, SHIFT_4,
	SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5,
	SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5,
	SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5,
	SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5, SHIFT_5,
	SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6,
	SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6,
	SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6,
	SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6, SHIFT_6,
	SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7,
	SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7,
	SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7,
	SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7, SHIFT_7,
};

#undef DD
#undef LL

////////////////////////////////////////////////////////////////////////
//      _                _
//   __| |___ _ __  __ _(_)_ _    _ _  __ _ _ __  ___ ___
//  / _` / _ \ '  \/ _` | | ' \  | ' \/ _` | '  \/ -_|_-<
//  \__,_\___/_|_|_\__,_|_|_||_| |_||_\__,_|_|_|_\___/__/
//

// Type of a domain name dope vector.
//
// This is a vector of the index of each label in an uncompressed wire
// format domain name. There can be at most 127 labels in a domain name.
//
typedef byte Dope[128];

// Fill in a domain name dope vector from an uncompressed wire format name.
//
// RFC 1035 section 3.1
//
// Returns the number of labels, not counting the root label.
//
// The last element of the dope vector is the position of the root label.
//
// size_t label = wire_dope(name, dope);
// assert(name[dope[label]] == 0);
//
// Crashes if name is not valid.
//
static inline size_t
wire_dope(const byte *name, Dope dope) {
	size_t label = 0;
	byte i = 0;
	// up to but not including the root label
	while(name[i] != 0) {
		assert(label < sizeof(Dope));
		dope[label++] = i;
		// label length
		assert(name[i] <= 63);
		// domain name length minus one for root
		assert(name[i] + 1 <= 255 - 1 - i);
		// plus 1 for length byte
		i += name[i] + 1;
	}
	// root label terminates the dope vector
	assert(label < sizeof(Dope));
	dope[label] = i;
	return(label);
}

// Convert an uncompressed wire format domain name into a trie lookup key.
//
// This involves reversing the order of the labels and converting byte
// values to bit numbers.
//
// Returns the length of the key.
//
// Crashes if name is not valid.
//
static size_t
wire_to_key(const byte *name, Key key) {
	Dope dope;
	size_t label = wire_dope(name, dope);
	size_t off = 0;
	while(label-- > 0) {
		byte i = dope[label] + 1;
		for(byte j = dope[label]; j > 0; i++, j--) {
			byte bit = byte_to_bit[name[i]];
			key[off++] = bit;
			if(byte_is_split(bit))
				key[off++] = lower_to_bit(name[i]);
		}
		key[off++] = SHIFT_NOBYTE;
	}
	// terminator is a double NOBYTE
	key[off] = SHIFT_NOBYTE;
	return(off);
}

// Like strcmp but for uncompressed wire format domain names.
//
// RFC 4034 section 6.1
//
// Crashes if name is not valid.
//
static int
wire_cmp(const byte *n, byte *m) {
	Dope nd, md;
	// label index
	size_t nl = wire_dope(n, nd);
	size_t ml = wire_dope(m, md);
	while(nl > 0 && ml > 0) {
		// label position
		byte ni = nd[--nl] + 1;
		byte mi = md[--ml] + 1;
		// label length
		byte nj = n[nd[nl]];
		byte mj = m[md[ml]];
		byte j = nj < mj ? nj : mj;
		while(j > 0) {
			byte nc = n[ni];
			byte mc = m[mi];
			if('A' <= nc && nc <= 'Z') nc += 'a' - 'A';
			if('A' <= mc && mc <= 'Z') mc += 'a' - 'A';
			if(nc < mc) return(-1);
			if(nc > mc) return(+1);
			ni++;
			mi++;
			j--;
		}
		if(nj < mj) return(-1);
		if(nj > mj) return(+1);
	}
	if(nl < ml) return(-1);
	if(nl > ml) return(+1);
	return(0);
}

// Are two uncompressed wire format domain names equal?
//
// Simpler than a full comparison, since we don't need to compare labels
// in reverse order.
//
static bool
wire_eq(const byte *n, const byte *m) {
	size_t i = 0;
	for(;;) {
		if(n[i] != m[i])
			return(false);
		size_t j = n[i++];
		if(j == 0)
			return(true);
		while(j > 0) {
			byte nc = n[i];
			byte mc = m[i];
			if('A' <= nc && nc <= 'Z') nc += 'a' - 'A';
			if('A' <= mc && mc <= 'Z') mc += 'a' - 'A';
			if(nc != mc) return(false);
			i++;
			j--;
		}
	}
}

#define ISDIGIT(c) ('0' <= (c) && (c) <= '9')

// Convert a presentation format domain name into a trie lookup key.
//
// Should have similar performance to wire_to_key() so we can use the
// existing test and benchmark harness, and get a reasonable idea of how
// well this works...
//
static size_t
text_to_key(const byte *name, Key key) {
	uint16_t lpos[128];
	uint16_t lend[128];
	size_t label = 0;
	uint16_t i = 0, j = 0;
	while(name[i] != '\0') {
		assert(name[i] != '.');
		lpos[label] = i;
		byte wirelen = 0;
		while(name[i] != '.' && name[i] != '\0') {
			if(name[i] != '\\') {
				i++;
			} else if(ISDIGIT(name[i+1])) {
				assert(ISDIGIT(name[i+2]));
				assert(ISDIGIT(name[i+3]));
				i += 4;
			} else {
				i += 2;
			}
			assert(++wirelen < 64);
		}
		lend[label] = i;
		label++;
		if(name[i] == '.')
			i++;
	}
	size_t off = 0;
	while(label-- > 0) {
		i = lpos[label];
		j = lend[label];
		while(i < j) {
			byte ch;
			if(name[i] != '\\') {
				ch = name[i++];
			} else if(ISDIGIT(name[i+1])) {
				ch = (name[i+1] - '0') * 100
				   + (name[i+2] - '0') * 10
				   + (name[i+3] - '0') * 1;
				i += 4;
			} else {
				ch = name[i+1];
				i += 2;
			}
			byte bit = byte_to_bit[ch];
			assert(off < sizeof(Key));
			key[off++] = bit;
			if(byte_is_split(bit))
				key[off++] = lower_to_bit(ch);
		}
		assert(off < sizeof(Key));
		key[off++] = SHIFT_NOBYTE;
	}
	// terminator
	key[off] = SHIFT_NOBYTE;
	return(off);
}

////////////////////////////////////////////////////////////////////////

// The number of bits set in a word.
//
typedef byte Weight;

static inline Weight
bmpcount(Node *n, word mask) {
	unsigned long long bmp = (unsigned long long)(n->index & mask);
	return((Weight)__builtin_popcountll(bmp));
}

static inline bool
isbranch(Node *n) {
	return(n->index & W1 << SHIFT_BRANCH);
}

static inline size_t
keyoff(Node *n) {
	return(n->index >> SHIFT_OFFSET);
}

static inline Shift
twigbit(Node *n, Key key, size_t len) {
	word off = keyoff(n);
	if(off < len) return(key[off]);
	else return(SHIFT_NOBYTE);
}

static inline bool
hastwig(Node *n, Shift bit) {
	return(n->index & W1 << bit);
}

static inline Weight
twigoff(Node *n, Shift bit) {
	return(bmpcount(n, bit_to_mask(bit)));
}

static inline Node *
twig(Node *n, Weight i) {
	return((Node *)n->ptr + i);
}

#define TWIGOFFMAX(off, max, n, bit) do {			\
		off = twigoff(n, bit);				\
		max = bmpcount(n, MASK_BITMAP);			\
	} while(0)

////////////////////////////////////////////////////////////////////////

extern bool
Tgetkv(Tbl *tbl, const char *name, size_t len, const char **pname, void **pval) {
	if(tbl == NULL)
		return(false);
	Node *n = &tbl->root;
	Key key;
	len = text_to_key((const byte *)name, key);
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		Shift bit = twigbit(n, key, len);
		if(!hastwig(n, bit))
			return(false);
		n = twig(n, twigoff(n, bit));
	}
	if(strcmp(name, n->ptr) != 0)
		return(false);
	*pname = n->ptr;
	*pval = (void *)n->index;
	return(true);
}

Tbl *
Tdelkv(Tbl *tbl, const char *name, size_t len, const char **pname, void **pval) {
	if(tbl == NULL)
		return(NULL);
	Node *n = &tbl->root, *p = NULL;
	Key key;
	len = text_to_key((const byte *)name, key);
	Shift bit = 0;
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		bit = twigbit(n, key, len);
		if(!hastwig(n, bit))
			return(tbl);
		p = n; n = twig(n, twigoff(n, bit));
	}
	if(strcmp(name, n->ptr) != 0)
		return(tbl);
	*pname = n->ptr;
	*pval = (void *)n->index;
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	n = p; p = NULL; // Because n is the usual name
	assert(bit != 0);
	Weight s, m; TWIGOFFMAX(s, m, n, bit);
	Node *twigs = n->ptr;
	if(m == 2) {
		// Move the other twig to the parent branch.
		*n = *twig(n, !s);
		free(twigs);
		return(tbl);
	}
	memmove(twigs+s, twigs+s+1, sizeof(Node) * (m - s - 1));
	n->index &= ~(W1 << bit);
	// We have now correctly removed the twig from the trie, so if
	// realloc() fails we can ignore it and continue to use the
	// slightly oversized twig array.
	twigs = realloc(twigs, sizeof(Node) * (m - 1));
	if(twigs != NULL) n->ptr = twigs;
	return(tbl);
}

Tbl *
Tsetl(Tbl *tbl, const char *name, size_t len, void *val) {
	// Ensure flag bits are zero.
	if(((word)val & MASK_FLAGS) != 0) {
		errno = EINVAL;
		return(NULL);
	}
	if(val == NULL)
		return(Tdell(tbl, name, len));
	Node newn = { .ptr = (void *)(word)name, .index = (word)val };
	// First leaf in an empty tbl?
	if(tbl == NULL) {
		tbl = malloc(sizeof(*tbl));
		if(tbl == NULL) return(NULL);
		tbl->root = newn;
		return(tbl);
	}
	Node *n = &tbl->root;
	Key newk;
	size_t newl = text_to_key((const byte *)name, newk);
	// Find the most similar leaf node in the trie. We will compare
	// its key with our new key to find the first differing byte,
	// which can be at a lower index than the point at which we
	// detect a difference.
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		Shift bit = twigbit(n, newk, newl);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigoff(n, bit) can cause
		// an out-of-bounds index if all set bits are less than bit.
		Weight i = hastwig(n, bit) ? twigoff(n, bit) : 0;
		n = twig(n, i);
	}
	// Do the keys differ, and if so, where?
	Key oldk;
	text_to_key((const byte *)n->ptr, oldk);
	size_t off;
	for(off = 0; off <= newl; off++) {
		if(newk[off] != oldk[off])
			goto newkey;
	}
	n->index = (word)val;
	return(tbl);
newkey:;
	Shift newb = newk[off], oldb = oldk[off];
	// Find where to insert a branch or grow an existing branch.
	n = &tbl->root;
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		if(off == keyoff(n))
			goto growbranch;
		if(off < keyoff(n))
			goto newbranch;
		Shift bit = twigbit(n, newk, newl);
		assert(hastwig(n, bit));
		n = twig(n, twigoff(n, bit));
	}
newbranch:;
	Node *twigs = malloc(sizeof(Node) * 2);
	if(twigs == NULL) return(NULL);
	Node oldn = *n; // Save before overwriting.
	n->index = (W1 << SHIFT_BRANCH)
		 | (W1 << newb)
		 | (W1 << oldb)
		 | (off << SHIFT_OFFSET);
	n->ptr = twigs;
	twigs[twigoff(n, newb)] = newn;
	twigs[twigoff(n, oldb)] = oldn;
	return(tbl);
growbranch:;
	assert(!hastwig(n, newb));
	Weight s, m; TWIGOFFMAX(s, m, n, newb);
	twigs = realloc(n->ptr, sizeof(Node) * (m + 1));
	if(twigs == NULL) return(NULL);
	memmove(twigs+s+1, twigs+s, sizeof(Node) * (m - s));
	twigs[s] = newn;
	n->ptr = twigs;
	n->index |= W1 << newb;
	return(tbl);
}

////////////////////////////////////////////////////////////////////////
