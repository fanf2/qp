#include <assert.h>
#include <inttypes.h>

typedef uintptr_t acbt_index;
typedef unsigned char byte;

// note that this is undefined when b is zero
static acbt_index acbt_gnuc_clz(byte b) {
	return(__builtin_clz(b) - __builtin_clz(0xFF));
}

static acbt_index acbt_portable_clz(byte b) {
	acbt_index i = 0;
	if(b & 0xF0) b &= 0xF0; else i += 4;
	if(b & 0xCC) b &= 0xCC; else i += 2;
	if(b & 0xAA) b &= 0xAA; else i += 1;
	return(i);
}

static acbt_index acbt_table_clz[] = {
    //  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, // 0
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 1
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 2
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 3
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 4
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 5
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 6
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 7
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // C
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // D
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // E
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F
};

int main(void) {
	for(unsigned b = 1; b < 256; b++) {
		unsigned t = acbt_table_clz[b];
		unsigned g = acbt_gnuc_clz(b);
		unsigned p = acbt_portable_clz(b);
		assert(g == t);
		assert(p == t);
	}
	return(0);
}
