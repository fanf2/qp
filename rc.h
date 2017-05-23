// rc.h: quintet bit popcount patricia tries, with rib compression
//
// This version uses somewhat different terminology than older
// variants. The location of a quintet in the key is now called its
// "offset", and the whole word containing the offset, bitmap, and tag
// bit is called the "index word" (by analogy with a database index).
// The precise quintet location is represented as a byte offset and a
// shift. Previously a flags field contained the tag and shift, but
// these are now separate.
//
// Instead of trying to use bit fields, this code uses accessor
// functions to split up a pair of words into their constituent parts.
// This should improve portability to machines with varying endianness
// and/or word size.
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

typedef struct Tbl {
	uint64_t index;
	void *ptr;
} Trie;

// index word layout

#define Tix_width_tag    1
#define Tix_width_shift  3
#define Tix_width_offset 28
#define Tix_width_bitmap 32

#define Tix_base_tag    0
#define Tix_base_shift  (Tix_base_tag  +   Tix_width_tag)
#define Tix_base_offset (Tix_base_shift +  Tix_width_shift)
#define Tix_base_bitmap (Tix_base_offset + Tix_width_offset)

#define Tix_place(field) ((uint64_t)(field) << Tix_base_##field)

#define Tunmask(field,index) ((uint)((index >> Tix_base_##field)	\
				     & ((1ULL << Tix_width_##field)	\
					- 1ULL)				\
				      ))

// sanity checks!

#ifndef static_assert
#define static_assert_cat(a,b) a##b
#define static_assert_name(line) static_assert_cat(static_assert_,line)
#define static_assert(must_be_true,message)				\
	static const void *static_assert_name(__LINE__)			\
		[must_be_true ? 2 : -1] = {				\
			message,					\
			&static_assert_name(__LINE__) }
#endif

static_assert(Tix_base_bitmap + Tix_width_bitmap == 64,
	      "index fields must fill a 64 bit word");

static_assert(Tunmask(bitmap,0x1234567800000000ULL) == 0x12345678,
	      "extracting the bitmap works");

static_assert(Tunmask(offset,0x0420ULL) == 0x42,
	      "extracting the offset works");

static_assert(Tunmask(shift,0xFEDCBAULL) == 5,
	      "extracting the shift works");

static inline bool
isbranch(Trie *t) {
	return(Tunmask(tag, t->index));
}

#define Tbranch(t) assert(isbranch(t))
#define Tleaf(t)  assert(!isbranch(t))

// accessor functions

#define Tcheck_get(type, tag, field, expr)	\
	static inline type			\
	tag##_##field(Trie *t) {		\
		tag(t);				\
		return(expr);			\
	}					\
	struct dummy

#define Tbranch_get(type, field)		\
	Tcheck_get(type, Tbranch, field,	\
		   Tunmask(field, t->index))

Tbranch_get(uint, shift);
Tbranch_get(uint, offset);
Tbranch_get(Tbitmap, bitmap);

Tcheck_get(Trie *,       Tbranch, twigs, t->ptr);
Tcheck_get(const char *, Tleaf,   key,   t->ptr);
Tcheck_get(void *,       Tleaf,   val,   (void*)t->index);

#define Tset_field(cast, elem, type, field)	\
	static inline void			\
	Tset_##field(Trie *t, type field) {	\
		t->elem = cast field;		\
	}					\
	struct dummy

Tset_field((void *),           ptr,   Trie *,       twigs);
Tset_field((void *)(uint64_t), ptr,   const char *, key);
Tset_field((uint64_t),         index, void *,       val);

static inline void
Tset_index(Trie *t, uint shift, uint offset, Tbitmap bitmap) {
	uint tag = 1;
	t->index = Tix_place(tag)
		 | Tix_place(shift)
		 | Tix_place(offset)
		 | Tix_place(bitmap);
}

static inline void
Tbitmap_add(Trie *t, Tbitmap bit) {
	Tset_index(t, Tbranch_shift(t), Tbranch_offset(t),
		   Tbranch_bitmap(t) | bit);
}

static inline void
Tbitmap_del(Trie *t, Tbitmap bit) {
	Tset_index(t, Tbranch_shift(t), Tbranch_offset(t),
		   Tbranch_bitmap(t) & ~bit);
}

//  ..key[o%5==0].. ..key[o%5==1].. ..key[o%5==2].. ..key[o%5==3].. ..key[o%5==4]..
// |               |               |               |               |               |
//  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
// |         |         |         |         |         |         |         |         |
//  shift=0   shift=5   shift=2   shift=7   shift=4   shift=1   shift=6   shift=3

static inline Tbitmap
nibbit(uint k, uint shift) {
	uint right = 16 - 5 - shift;
	return(1U << ((k >> right) & 0x1FU));
}

static inline Tbitmap
twigbit(Trie *t, const char *key, size_t len) {
	uint o = Tbranch_offset(t);
	if(o >= len) return(1);
	uint k = (uint)(key[o] & 0xFF) << 8U;
	if(k) k |= (uint)(key[o+1] & 0xFF);
	return(nibbit(k, Tbranch_shift(t)));
}

static inline bool
hastwig(Trie *t, Tbitmap bit) {
	return(Tbranch_bitmap(t) & bit);
}

static inline uint
twigoff(Trie *t, Tbitmap b) {
	return(popcount(Tbranch_bitmap(t) & (b-1)));
}

static inline Trie *
twig(Trie *t, uint i) {
	return(&Tbranch_twigs(t)[i]);
}

#define TWIGOFFMAX(off, max, t, b) do {			\
		off = twigoff(t, b);			\
		max = popcount(Tbranch_bitmap(t));	\
	} while(0)
