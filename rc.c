// rc.h: quintet bit popcount patricia tries, with rib compression
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
#include "rc.h"

bool
Tgetkv(Tbl *t, const char *key, size_t len, const char **pkey, void **pval) {
	if(t == NULL)
		return(false);
	while(isbranch(t)) {
		// step into trunk
		Trie *twigs = t->ptr;
		__builtin_prefetch(twigs);
		Tindex i = t->index;
		t = NULL;
		for(;;) {
			// examine this branch
			byte n = nibble(i, key, len);
			Tbitmap b = 1U << n;
			if(hastwig(i, b)) {
				t = twigs + twigoff(i, b);
				break; // to outer loop
			} else if(Tindex_concat(i) == n) {
				uint max = popcount(Tindex_bitmap(i));
				// step along trunk
				Tindex *ip = (void*)(twigs + max); i = *ip;
				twigs = (void*)(ip+1);
				assert(Tindex_branch(i));
				continue; // inner loop
			} else {
				return(false);
			}
		}
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return(false);
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	return(true);
}

static bool
next_rec(Trie *t, const char **pkey, size_t *plen, void **pval) {
	Tindex i = t->index;
	if(Tindex_branch(i)) {
		// Recurse to find either this leaf (*pkey != NULL)
		// or the next one (*pkey == NULL).
		Tbitmap b = 1U << nibble(i, *pkey, *plen);
		uint s, m; TWIGOFFMAX(s, m, i, b);
		for(; s < m; s++)
			if(next_rec(Tbranch_twigs(t)+s, pkey, plen, pval))
				return(true);
		return(false);
	}
	// We have found the next leaf.
	if(*pkey == NULL) {
		*pkey = Tleaf_key(t);
		*plen = strlen(*pkey);
		*pval = Tleaf_val(t);
		return(true);
	}
	// We have found this leaf, so start looking for the next one.
	if(strcmp(*pkey, Tleaf_key(t)) == 0) {
		*pkey = NULL;
		*plen = 0;
		return(false);
	}
	// No match.
	return(false);
}

bool
Tnextl(Tbl *tbl, const char **pkey, size_t *plen, void **pval) {
	if(tbl == NULL) {
		*pkey = NULL;
		*plen = 0;
		return(NULL);
	}
	return(next_rec(tbl, pkey, plen, pval));
}

static size_t
trunksize(Trie *t) {
	size_t s = 0;
	Tindex i = t->index;
	Trie *twigs = t->ptr;
	for(;;) {
		assert(Tindex_branch(i));
		uint max = popcount(Tindex_bitmap(i));
		s += max * sizeof(Trie);
		if(trunkend(i))
			return(s);
		Tindex *ip = (void*)(twigs + max);
		s += sizeof(Tindex);
		i = *ip++;
		twigs = (void*)ip;
	}
}

static void *
mdelete(void *vbase, size_t size, void *vsplit, size_t loss) {
	byte *base = vbase, *split = vsplit;
	assert(size > loss);
	assert(base <= split);
	assert(split < base + size);
	assert(split + loss <= base + size);
	size_t suffix = (base + size) - (split + loss);
	memmove(split, split + loss, suffix);
	// If realloc() fails, continue to use the oversized allocation.
	void *maybe = realloc(base, size - loss);
	return(maybe ? maybe : base);
}

static void *
mtrimsert(void *vbase, size_t size, size_t trim, void *vsplit,
	  void *vinsert, size_t gain) {
	byte *base = vbase, *split = vsplit, *insert = vinsert;
	assert(size > trim);
	assert(base <= split);
	assert(split < base + size);
	size -= trim;
	size_t prefix = split - base;
	size_t suffix = (base + size) - split;
	void *maybe = realloc(base, size + gain);
	if(maybe != NULL)
		base = maybe;
	else if(gain > trim)
		return(NULL);
	// If realloc() fails, continue to use the old allocation
	// provided we will not gain more than we trimmed.
	split = base + prefix;
	memmove(split + gain, split, suffix);
	memmove(split, insert, gain);
	return(base);
}

static void *
minsert(void *base, size_t size, void *split, void *insert, size_t gain) {
	return(mtrimsert(base, size, 0, split, insert, gain));
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, size_t len, const char **pkey, void **pval) {
	if(tbl == NULL)
		return(NULL);
	// parent and grandparent twigs of the current trunk
	Trie *p = NULL, *gp = NULL;
	// i abbreviates *ip; b is always wrt i
	Tindex *ip, i;
	Tbitmap b;
	// i and twigs start off as elements of p
	// then bump along the trunk together
	Trie *twigs;
	// previous ip
	Tindex *pip = NULL;
	// t is a twig of the current branch we might delete or follow
	Trie *t = tbl;
	while(isbranch(t)) {
		// step into next trunk
		gp = p; p = t; t = NULL;
		pip = ip; ip = &p->index; i = *ip;
		twigs = p->ptr;
		__builtin_prefetch(twigs);
		for(;;) {
			// examine this branch
			byte n = nibble(i, key, len);
			b = 1U << n;
			if(hastwig(i, b)) {
				t = twigs + twigoff(i, b);
				break; // to outer loop
			} else if(Tindex_concat(i) == n) {
				uint max = popcount(Tindex_bitmap(i));
				// step along trunk
				pip = ip; ip = (void*)(twigs + max); i = *ip;
				twigs = (void*)(ip+1);
				assert(Tindex_branch(i));
				continue; // inner loop
			} else {
				return(tbl);
			}
		}
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return(tbl);
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	if(p == NULL) {
		free(tbl);
		return(NULL);
	}
	Trie *trunk = Tbranch_twigs(p);
	uint s = trunksize(p);
	assert(trunk <= t && t < trunk+s);
	uint m = popcount(Tindex_bitmap(i));
	if(m == 1) {
		// Our twig is the last in a rib branch, so we need
		// to remove the whole branch from the trunk. The
		// following index word, for the concatenated branch,
		// needs to be moved to where our index word was.
		*ip = *(Tindex *)(t + 1);
		// If our branch was indirect, this will update the pointer
		// properly; if our branch was concatenated we do not have a
		// pointer to update.
		Tset_twigs(p, mdelete(trunk, s, t,
				      sizeof(Trie) + sizeof(Tindex)));
		return(tbl);
	}
	if(m == 2 && trunkend(i)) {
		// We need to move the other twig into its parent.
		// There should be two leaves in this situation, but if
		// realloc() fails when we want to concatenate two trunks, we
		// will be unable to keep the trie as tight as we want. So we
		// need to allow for trunks that end with one branch.
		if(ip != &p->index) {
			// We were concatenated, so we need to shift the
			// other twig into the preceding twig array. This
			// will normally convert the preceding twig array
			// into all twigs. The abnormal case should be
			// vanishingly rare, so don't worry about it.
			void *end = (byte *)trunk + s;
			Trie save = t[t + 1 == end ? -1 : +1];
			// Update preceding index.
			i = *pip;
			b = 1U << Tindex_concat(i);
			t = (Trie *)(pip + 1) + twigoff(i, b);
			*pip = Tbitmap_add(i, b);
			Tset_twigs(p, mtrimsert(trunk, s,
						sizeof(Tindex) + sizeof(Trie) * 2,
						t, &save, sizeof(Trie)));
			return(tbl);
		}
		// We were an indirect branch, so p is the parent.
		*p = t[t == trunk ? +1 : -1];
		free(trunk);
		// We probably changed a branch into a leaf, which means
		// there might be only one other branch in the twig array
		// containing p, in which case we need to concatenate the
		// other branch's trunk onto its parent. We need to recover
		// the location of the twig array so we can scan it.
		i = *pip;
		b = 1U << nibble(i, key, len);
		twigs = p - twigoff(i, b);
		m = popcount(Tindex_bitmap(i));
		uint n = 0;
		for(t = twigs; t < twigs + m; t++)
			if(isbranch(t))
				p = t, n++;
		if(n != 1)
			return(tbl);
		// Now we need to concatenate p's trunk onto gp's trunk.
		uint gs = trunksize(gp);
		s = trunksize(p);
		Trie save = *p;
		trunk = Tbranch_twigs(gp);
		size_t split = (byte*)p - (byte*)trunk;
		byte *gt = realloc(trunk, gs + s
				   - sizeof(Trie) + sizeof(Tindex));
		if(gt == NULL) {
			// Well, we can't concatenate them this time,
			// maybe there will be another opportunity.
			return(tbl);
		}
		// Delete p from the twigs
		memmove(gt + split, gt + split + sizeof(Trie),
				    gs - split - sizeof(Trie));
		// Was probably moved by realloc()
		pip = (Tindex*)( gt + ((byte*)pip - (byte*)trunk) );
		// The index must say p is a concatenated branch
		n = nibble(i, key, len);
		b = 1U << n;
		*pip = Tindex_new(Tindex_shift(i), Tindex_offset(i),
				  n, Tindex_bitmap(i) & ~b);
		// Place concatenated index word
		gs -= sizeof(Trie);
		ip = (Tindex*)(gt + gs);
		*ip = save.index;
		// Concatenate!
		memmove(ip+1, save.ptr, s);
		free(save.ptr);
		return(tbl);
	}
	// Usual case
	*ip = Tbitmap_del(i, b);
	Tset_twigs(p, mdelete(trunk, s, t, sizeof(Trie)));
	return(tbl);
}

Tbl *
Tsetl(Tbl *tbl, const char *key, size_t len, void *val) {
	if(Tindex_branch((Tindex)val) || len > Tmaxlen) {
		errno = EINVAL;
		return(NULL);
	}
	if(val == NULL)
		return(Tdell(tbl, key, len));
	// First leaf in an empty tbl?
	if(tbl == NULL) {
		tbl = malloc(sizeof(*tbl));
		if(tbl == NULL) return(NULL);
		Tset_key(tbl, key);
		Tset_val(tbl, val);
		return(tbl);
	}
	Trie *t = tbl;
	// Find the most similar leaf node in the trie. We will compare
	// its key with our new key to find the first differing nibble,
	// which can be at a lower index than the point at which we
	// detect a difference.
	while(isbranch(t)) {
		// step into trunk
		Trie *twigs = t->ptr;
		__builtin_prefetch(twigs);
		Tindex i = t->index;
		t = NULL;
		for(;;) {
			// examine this branch
			byte n = nibble(i, key, len);
			Tbitmap b = 1U << n;
			if(hastwig(i, b)) {
				t = twigs + twigoff(i, b);
				break; // to outer loop
			} else if(Tindex_concat(i) == n) {
				uint max = popcount(Tindex_bitmap(i));
				// step along trunk
				Tindex *ip = (void*)(twigs + max); i = *ip;
				twigs = (void*)(ip+1);
				assert(Tindex_branch(i));
				continue; // inner loop
			} else {
				// Even if our key is missing from this branch
				// we need to keep iterating down to a leaf. It
				// doesn't matter which twig we choose since the
				// keys are all the same up to this index.
				t = twigs;
				break; // to outer loop
			}
		}
	}
	// Do the keys differ, and if so, where?
	uint off, xor, shf;
	const char *tkey = Tleaf_key(t);
	for(off = 0; off <= len; off++) {
		xor = (byte)key[off] ^ (byte)tkey[off];
		if(xor != 0) goto newkey;
	}
	Tset_val(t, val);
	return(tbl);
newkey:; // We have the branch's byte index; what is its chunk index?
	uint bit = off * 8 + (uint)__builtin_clz(xor) + 8 - sizeof(uint) * 8;
	uint qo = bit / 5;
	off = qo * 5 / 8;
	shf = qo * 5 % 8;
	// re-index keys with adjusted offset
	Tbitmap nb = 1U << knybble(key,off,shf);
	Tbitmap tb = 1U << knybble(tkey,off,shf);
	// Prepare the new leaf.
	Trie nt;
	Tset_key(&nt, key);
	Tset_val(&nt, val);
	// Find where to insert a branch or grow an existing branch.
	t = tbl;
	Tindex i = 0;
	while(isbranch(t)) {
		// step into trunk
		Trie *twigs = t->ptr;
		__builtin_prefetch(twigs);
		i = t->index;
		t = NULL;
		for(;;) {
			// examine this branch
			if(off == Tindex_offset(i) && shf == Tindex_shift(i))
				goto growbranch;
			if(off == Tindex_offset(i) && shf < Tindex_shift(i))
				goto newbranch;
			if(off < Tindex_offset(i))
				goto newbranch;
			byte n = nibble(i, key, len);
			Tbitmap b = 1U << n;
			if(hastwig(i, b)) {
				t = twigs + twigoff(i, b);
				break; // to outer loop
			} else if(Tindex_concat(i) == n) {
				uint max = popcount(Tindex_bitmap(i));
				// step along trunk
				Tindex *ip = (void*)(twigs + max); i = *ip;
				twigs = (void*)(ip+1);
				assert(Tindex_branch(i));
				continue; // inner loop
			} else {
				assert(false);
			}
		}
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == NULL) return(NULL);
	i = Tindex_new(shf, off, nb | tb);
	twigs[twigoff(i, nb)] = nt;
	twigs[twigoff(i, tb)] = *t;
	Tset_twigs(t, twigs);
	Tset_index(t, i);
	return(tbl);
growbranch:;
	assert(!hastwig(i, nb));
	uint s, m; TWIGOFFMAX(s, m, i, nb);
	twigs = realloc(Tbranch_twigs(t), sizeof(Trie) * (m + 1));
	if(twigs == NULL) return(NULL);
	memmove(twigs+s+1, twigs+s, sizeof(Trie) * (m - s));
	memmove(twigs+s, &nt, sizeof(Trie));
	Tset_twigs(t, twigs);
	Tset_index(t, Tbitmap_add(i, nb));
	return(tbl);
}
