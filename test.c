// test.c: test table implementations.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#define _WITH_GETLINE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Tbl.h"

static const char *progname;

static void
die(const char *cause) {
	fprintf(stderr, "%s: %s: %s\n", progname, cause, strerror(errno));
	exit(1);
}

static void
usage(void) {
	fprintf(stderr,
"usage: %s [input]\n"
"	The input is a series of lines starting with a + or a - to add\n"
"	or delete a key from the table. The rest of the line is the key.\n"
	    , progname);
	exit(1);
}

static void
trace(Tbl *t, int s, const char *key) {
//	printf("%c%s\n", s, key);
//	Tdump(t);
}

int
main(int argc, char *argv[]) {
	progname = argv[0];
	if(argc > 2)
		usage();
	if(argc == 2) {
		if(argv[1][0] == '-')
			usage();
		if(freopen(argv[1], "r", stdin) == NULL)
			die("open");
	}
	Tbl *t = NULL;
	for (;;) {
		char *key = NULL;
		size_t len = 0;
		int s = getchar();
		if(s < 0) break;
		ssize_t n = getline(&key, &len, stdin);
		if(n < 0) break;
		else len = (size_t)n;
		if(len > 0 && key[len-1] == '\n')
			key[--len] = '\0';
		switch(s) {
		default:
			usage();
		case('*'):
			if(Tget(t, key))
				putchar('*');
			else
				putchar('=');
			continue;
		case('+'):
			errno = 0;
			void *val = Tget(t, key);
			t = Tsetl(t, key, len, val == NULL ? key : val);
			if(t == NULL)
				die("Tbl");
			if(!val)
				trace(t, s, key);
			else
				free(key);
			continue;
		case('-'):
			errno = 0;
			const char *rkey = NULL;
			void *rval = NULL;
			t = Tdelkv(t, key, len, &rkey, &rval);
			if(t == NULL && errno != 0)
				die("Tbl");
			if(rkey)
				trace(t, s, key);
			free(key);
			free(rkey);
			continue;
		}
	}
	putchar('\n');
	if(ferror(stdin))
		die("read");
	size_t size, depth, leaves;
	const char *type;
	Tsize(t, &type, &size, &depth, &leaves);
	size_t overhead = size / sizeof(void*) - 2 * leaves;
	fprintf(stderr, "SIZE %s leaves=%zu branches=%zu overhead=%.2f depth=%.2f\n",
		type, leaves, overhead,
		(double)overhead / leaves,
		(double)depth / leaves);
	const char *key = NULL;
	void *val = NULL, *prev = NULL;
	while(Tnext(t, &key, &val)) {
		assert(key == val);
		puts(key);
		if(prev) {
			t = Tdel(t, prev);
			trace(t, '!', prev);
			free(prev);
		}
		prev = val;
	}
	if(prev) {
		t = Tdel(t, prev);
		free(prev);
	}
	return(0);
}
