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
#include "dns.h"

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
    (if (= 0 (% byte 8))
      (insert "\t"))
    (cond
      ((and (>= byte 32) (<  byte ?-))	(insert "SHIFTa1"))
      ((= byte ?-)			(insert "SHYPHEN"))
      ((= byte ?.)			(insert "SHIFDOT"))
      ((= byte ?/)			(insert "SHSLASH"))
;;    ((and (>  byte ?-) (<  byte ?0))	(insert "SHIFTb1"))
      ((and (>= byte ?0) (<= byte ?9))	(insert "DD('" (byte-to-string byte) "')"))
      ((and (>  byte ?9) (<  byte 64))	(insert "SHIFTc1"))
      ((and (>= byte ?A) (<= byte ?Z))	(insert "LL('" (byte-to-string (+ 32 byte)) "')"))
      ((= byte ?_)			(insert "UNDERBAR"))
      ((= byte ?`)			(insert "BACKQUO"))
      ((and (>= byte ?a) (<= byte ?z))	(insert "LL('" (byte-to-string byte) "')"))
      (t				(insert "SHIFT_" (number-to-string (/ byte 32)))))
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
	SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHYPHEN, SHIFDOT, SHSLASH,
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

// Same again, but case-sensitive.
//
static const Shift case_byte_to_bit[256] = {
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0, SHIFT_0,
	SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1,
	SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHIFTa1, SHYPHEN, SHIFDOT, SHSLASH,
	DD('0'), DD('1'), DD('2'), DD('3'), DD('4'), DD('5'), DD('6'), DD('7'),
	DD('8'), DD('9'), SHIFTc1, SHIFTc1, SHIFTc1, SHIFTc1, SHIFTc1, SHIFTc1,
	SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2,
	SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2,
	SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2,
	SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, SHIFT_2, UNDERBAR,
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

// The code in this section is a bit sketchy. The test and benchmark
// harnesses don't work well with domain names, so we're using textual
// names for a proof of concept. We'll do wire format names properly
// if/when this code is integrated into some less experimental software.

// Type of a domain name dope vector.
//
// This is a vector of the index of each label in an uncompressed wire
// format domain name. Domain names are up to 255 bytes long so a byte is
// big enough to hold an index. There can be at most 127 labels in a domain
// name.
//
// I'm misusing the term "dope vector", which originally comes from
// multi-dimensional array implementations in which the dope vecor
// contains the bounds and stride of each dimension of the array.
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
				key[off++] = split_to_bit(name[i]);
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

// Convert a presentation format domain name into a trie lookup key
// (in standard lexical order).
//
// Should have similar performance to wire_to_key() so we can use the
// existing test and benchmark harness, and get a reasonable idea of how
// well this works...
//
static size_t
stdtext_to_key(const byte *name, Key key) {
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
				key[off++] = split_to_bit(ch);
		}
		assert(off < sizeof(Key));
		key[off++] = SHIFT_NOBYTE;
	}
	// terminator
	key[off] = SHIFT_NOBYTE;
	return(off);
}

// Convert a presentation format domain name into a trie lookup key
// (in non-standard case-sensitive left-to-right order).
//
// This should produce exactly equal output to other trie implementations.
//
static size_t
text_to_key(const byte *name, Key key) {
	size_t off = 0;
	while(*name != '\0') {
		byte ch = *name++;
		byte bit = case_byte_to_bit[ch];
		assert(off < sizeof(Key));
		key[off++] = bit;
		if(byte_is_split(bit))
			key[off++] = split_to_bit(ch);
	}
	// terminator
	key[off] = SHIFT_NOBYTE;
	return(off);
}

////////////////////////////////////////////////////////////////////////
//   _        _    _         _   ___ ___
//  | |_ __ _| |__| |___    /_\ | _ \_ _|
//  |  _/ _` | '_ \ / -_)  / _ \|  _/| |
//   \__\__,_|_.__/_\___| /_/ \_\_| |___|
//

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
	if(strcmp(name, n->ptr) != 0) /////
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
	if(strcmp(name, n->ptr) != 0) /////
		return(tbl);
	*pname = n->ptr;
	*pval = (void *)n->index;
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	n = p; p = NULL; // Because n is the usual name
	assert(bit != 0);
	Weight s = twigoff(n, bit);
	Weight m = twigmax(n);
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
	// Find a nearby leaf node in the trie.
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		n = twig(n, neartwig(n, twigbit(n, newk, newl)));
	}
	// Do the keys differ, and if so, where?
	Key oldk;
	text_to_key(n->ptr, oldk);
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
	Weight s = twigoff(n, newb);
	Weight m = twigmax(n);
	twigs = realloc(n->ptr, sizeof(Node) * (m + 1));
	if(twigs == NULL) return(NULL);
	memmove(twigs+s+1, twigs+s, sizeof(Node) * (m - s));
	twigs[s] = newn;
	n->ptr = twigs;
	n->index |= W1 << newb;
	return(tbl);
}

bool
Tnextl(Tbl *tbl, const char **pname, size_t *plen, void **pval) {
	if(tbl == NULL) {
		*pname = NULL;
		*plen = 0;
		return(false);
	}
	Node *n = &tbl->root;
	Key newk;
	size_t newl;
	if(*pname == NULL)
		newl = text_to_key((const byte *)"", newk);
	else
		newl = text_to_key((const byte *)*pname, newk);
	// Find a nearby leaf node in the trie.
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		n = twig(n, neartwig(n, twigbit(n, newk, newl)));
	}
	// Do the keys differ, and if so, where?
	Key oldk;
	text_to_key(n->ptr, oldk);
	size_t off;
	for(off = 0; off <= newl; off++) {
		if(newk[off] != oldk[off])
			break;
	}
	// Walk down again and this time keep track of adjacent nodes
	n = &tbl->root;
	Node *prev, *next;
	if(*pname == NULL)
		next = prev = n;
	else
		next = prev = NULL;
	while(isbranch(n)) {
		__builtin_prefetch(n->ptr);
		if(off <= keyoff(n))
			break;
		Shift newb = twigbit(n, newk, newl);
		assert(hastwig(n, newb));
		Weight s = twigoff(n, newb);
		Weight m = twigmax(n) - 1;
		if(s > 0) prev = twig(n, s - 1);
		if(s < m) next = twig(n, s + 1);
		n = twig(n, s);
	}
	// walk prev and next nodes down to their leaves
	while(prev != NULL && isbranch(prev)) {
		__builtin_prefetch(prev->ptr);
		prev = twig(prev, twigmax(prev) - 1);
	}
	while(next != NULL && isbranch(next)) {
		__builtin_prefetch(next->ptr);
		next = twig(next, 0);
	}
	if(next != NULL) {
		*pname = next->ptr;
		*plen = strlen(*pname); /////
		*pval = (void *)next->index;
		return(true);
	} else {
		*pname = NULL;
		*plen = 0;
		return(false);
	}
}

////////////////////////////////////////////////////////////////////////
