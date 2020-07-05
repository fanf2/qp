// dns-debug.c: DNS-trie debug support
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "Tbl.h"
#include "dns.h"

static void
print_bit(Shift bit) {
	if(bit == SHIFT_0) printf("^0/");
	if(bit == SHIFTa1) printf("^1a/");
	if(bit == SHYPHEN) printf("-/");
	if(bit == SHIFDOT) printf("./");
	if(bit == SHSLASH) printf("//");
//	if(bit == SHIFTb1) printf("^1b/");
	if(SHIFT_DIGIT <= bit && bit <= TOP_DIGIT)
		printf("%c/", '0' + bit - SHIFT_DIGIT);
	if(bit == SHIFTc1) printf("^1c/");
	if(bit == SHIFT_2) printf("^2/");
	if(bit == UNDERBAR) printf("_/");
	if(bit == BACKQUO) printf("`/");
	if(SHIFT_LETTER <= bit && bit <= TOP_LETTER)
		printf("%c/", 'a' + bit - SHIFT_LETTER);
	if(bit == SHIFT_2) printf("^2/");
	if(bit == SHIFT_3) printf("^3/");
	if(bit == SHIFT_4) printf("^4/");
	if(bit == SHIFT_5) printf("^5/");
	if(bit == SHIFT_6) printf("^6/");
	if(bit == SHIFT_7) printf("^7/");
	printf("%d", bit - SHIFT_LOWER);
}

static void
print_bitmap(Node *n) {
	char sep = '(';
	if(hastwig(n, SHIFT_NOBYTE)) {
		printf("(NO");
		sep = ',';
	}
	for(byte bit = SHIFT_0; bit < SHIFT_OFFSET; bit++) {
		if(!hastwig(n, bit))
			continue;
		putchar(sep);
		print_bit(bit);
		sep = ',';
	}
	printf(")\n");
}

static void
dump_rec(Node *n, int d) {
	if(isbranch(n)) {
		printf("Tdump%*s branch %p %zu %zu", d, "", n,
		       (size_t)n->index & MASK_FLAGS, keyoff(n));
		print_bitmap(n);
		int dd = (int)keyoff(n) * 2 + 2;
		assert(dd > d);
		for(Shift bit = SHIFT_NOBYTE; bit < SHIFT_OFFSET; bit++) {
			if(hastwig(n, bit)) {
				printf("Tdump%*s twig ", d, "");
				print_bit(bit);
				putchar('\n');
				dump_rec(twig(n, twigoff(n, bit)), dd);
			}
		}
	} else {
		printf("Tdump%*s leaf %p\n", d, "", n);
		printf("Tdump%*s leaf key %p %s\n", d, "",
		       n->ptr, n->ptr);
		printf("Tdump%*s leaf val %zx\n", d, "",
		       (size_t)n->index);
	}
}

void
Tdump(Tbl *tbl) {
	printf("Tdump root %p\n", tbl);
	if(tbl != NULL)
		dump_rec(&tbl->root, 0);
}

static void
size_rec(Node *n, size_t d,
    size_t *rsize, size_t *rdepth, size_t *rbranches, size_t *rleaves) {
	*rsize += sizeof(*n);
	if(isbranch(n)) {
		*rbranches += 1;
		for(Shift bit = SHIFT_NOBYTE; bit < SHIFT_OFFSET; bit++) {
			if(hastwig(n, bit))
				size_rec(twig(n, twigoff(n, bit)),
				    d+1, rsize, rdepth, rbranches, rleaves);
		}
	} else {
		*rleaves += 1;
		*rdepth += d;
	}
}

void
Tsize(Tbl *tbl, const char **rtype,
    size_t *rsize, size_t *rdepth, size_t *rbranches, size_t *rleaves) {
	*rtype = "dns";
	*rsize = *rdepth = *rbranches = *rleaves = 0;
	if(tbl != NULL)
		size_rec(&tbl->root, 0, rsize, rdepth, rbranches, rleaves);
}
