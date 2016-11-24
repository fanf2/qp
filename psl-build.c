// Compress the Public Suffix List in the style of a QP trie.

#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tracing = false;

#define trace(fmt, ...) do {					\
		if(tracing)					\
			fprintf(stderr, fmt, __VA_ARGS__);	\
	} while(0)

typedef unsigned char byte;
typedef unsigned int uint;

typedef uint64_t Tbitmap;

typedef struct Trie {
	Tbitmap bmp : 39;
	union {
		struct Trie *twigs;
		const char *key;
	};
} Trie;

static inline uint
popcount(Tbitmap w) {
	return((uint)__builtin_popcountll(w));
}

static char
ldh2i(char c) {
	if(c == '.') return(0);
	if(c == '-') return(1);
	if('0' <= c && c <= '9') return(c - '0' + 2);
	if('A' <= c && c <= 'Z') return(c - 'A' + 2 + 10);
	if('a' <= c && c <= 'z') return(c - 'a' + 2 + 10);
	// this is last so we don't have to include it in the twigs
	if(c == '\0') return(2 + 10 + 26);
	assert(0 && "invalid character in domain name");
}

static Tbitmap
ldh2bit(char c) {
	return(1ULL << (byte)ldh2i(c));
}

static inline Tbitmap
twigbit(uint i, const char *key, size_t len) {
	if(i >= len) return(ldh2bit(0));
	return(ldh2bit(key[i]));
}

static inline bool
hastwig(Trie *t, Tbitmap bit) {
	return(t->bmp & bit);
}

static inline uint
twigoff(Trie *t, Tbitmap b) {
	return(popcount(t->bmp & (b-1)));
}

static inline Trie *
twig(Trie *t, uint i) {
	return(&t->twigs[i]);
}

static bool
isbranch(Trie *t) {
	return(t->bmp != 0);
}

static void
Tdump(Trie *t, uint d) {
	if(!tracing) return;
	if(!isbranch(t)) {
		trace("%-16p %d %*c %s\n", (void *)t, d, d+1, '*', t->key);
		return;
	}
	trace("%16jo %d\n", (uintmax_t)t->bmp, d);
	Tbitmap b;
	if(hastwig(t, b = ldh2bit(0))) {
		trace("%-16p %d %*c\n", (void *)t, d, d+1, '!');
		Tdump(twig(t, twigoff(t, b)), d+1);
	}
	for(const char *s = ".-0123456789abcdefghijklmnopqrstuvwxyz";
	    *s != '\0'; s++) {
		if(hastwig(t, b = ldh2bit(*s))) {
			trace("%-16p %d %*c\n", (void *)t, d, d+1, *s);
			Tdump(twig(t, twigoff(t, b)), d+1);
		}
	}
}

static bool
Tfind(Trie *t, const char *key, size_t len) {
	for(uint i = 0; isbranch(t); i++) {
		__builtin_prefetch(t->twigs);
		Tbitmap b = twigbit(i, key, len);
		if(!hastwig(t, b))
			return(false);
		t = twig(t, twigoff(t, b));
	}
	return(strcmp(key, t->key) == 0);
}

static void
Tadd(Trie *t, const char *key, size_t len) {
	// First leaf in an empty tbl?
	if(t->key == NULL) {
		trace("1st 0 %s\n", key);
		t->key = key;
		return;
	}
	uint i = 0;
	while(isbranch(t)) {
		__builtin_prefetch(t->twigs);
		Tbitmap b = twigbit(i, key, len);
		if(!hastwig(t, b)) {
			trace("gro %d %s\n", i, key);
			uint s = twigoff(t, b);
			uint m = popcount(t->bmp);
			Trie *twigs = realloc(t->twigs, sizeof(Trie) * (m + 1));
			assert(twigs != NULL);
			memmove(twigs+s+1, twigs+s, sizeof(Trie) * (m - s));
			twigs[s] = (Trie){ .key = key };
			t->twigs = twigs;
			t->bmp |= b;
			return;
		}
		t = twig(t, twigoff(t, b));
		i++;
	}
	if(strcmp(key, t->key) == 0)
		return;
	Trie t1 = *t;
	Trie t2 = { .key = key };
	size_t len1 = strlen(t1.key);
	size_t len2 = len;
	while(t1.key[i] == t2.key[i]) {
		trace("ext %d %s %s\n", i, t1.key, t2.key);
		t->bmp = twigbit(i, key, len);
		t->twigs = malloc(sizeof(Trie));
		assert(t->twigs != NULL);
		t = t->twigs;
		i++;
	}
	trace("new %d %s %s\n", i, t1.key, t2.key);
	Trie *twigs = malloc(sizeof(Trie) * 2);
	assert(twigs != NULL);
	Tbitmap b1 = twigbit(i, t1.key, len1);
	Tbitmap b2 = twigbit(i, t2.key, len2);
	t->twigs = twigs;
	t->bmp = b1 | b2;
	trace("%16jo %zu\n", (uintmax_t)b1, len1);
	trace("%16jo %zu\n", (uintmax_t)b2, len2);
	trace("%16jo\n", (uintmax_t)t->bmp);
	*twig(t, twigoff(t, b1)) = t1;
	*twig(t, twigoff(t, b2)) = t2;
	return;
}

static void
Tprint(Trie *t) {
	if(!isbranch(t)) {
		printf("%s\n", t->key);
		return;
	}
	Tbitmap b;
	if(hastwig(t, b = ldh2bit(0)))
		Tprint(twig(t, twigoff(t, b)));
	for(const char *s = "-.0123456789abcdefghijklmnopqrstuvwxyz";
	    *s != '\0'; s++)
		if(hastwig(t, b = ldh2bit(*s)))
			Tprint(twig(t, twigoff(t, b)));
}

static void
Tcount(Trie *t, uint d, uint *nodes, uint *string) {
	if(isbranch(t)) {
		*nodes += 1;
		// end-of-string nodes are omitted
		uint m = popcount(t->bmp & ~ldh2bit(0));
		for(uint i = 0; i < m; i++)
			Tcount(&t->twigs[i], d+1, nodes, string);
	} else {
		size_t len = strlen(t->key);
		assert(len >= d);
		*nodes += 1;
		// short keys are stored in the node
		if(len > 6)
			*string += len - d;
	}
x}

int main(void) {
	char *buf = NULL;
	size_t size = 0;
	ssize_t len;

	Trie *t = malloc(sizeof(*t));
	assert(t != NULL);
	*t = (Trie){ .key = NULL };

	while((len = getline(&buf, &size, stdin)) >= 0) {
		if(len > 1 && buf[len-1] == '\n')
			buf[--len] = '\0';
		Tadd(t, strdup(buf), (size_t)len);
		Tdump(t, 0);
	}

	Tprint(t);

	uint nodes = 0, string = 0;
	Tcount(t, 0, &nodes, &string);
	fprintf(stderr, "%u nodes, %u string, %u bytes\n",
		nodes, string, nodes * 7 + string);

	return(0);
}
