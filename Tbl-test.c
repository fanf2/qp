// Tbl-test.c: test table implementations.
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
				putchar('+');
			else
				putchar('-');
			continue;
		case('+'):
			errno = 0;
			void *val = Tget(t, key);
			if(val == NULL)
				val = key;
			t = Tsetl(t, key, len, val);
			if(t == NULL)
				die("Tbl");
			trace(t, s, key);
			if(val != key)
				free(key);
			continue;
		case('-'):
			errno = 0;
			const char *rkey = NULL;
			void *rval = NULL;
			t = Tdelkv(t, key, len, &rkey, &rval);
			if(t == NULL && errno != 0)
				die("Tbl");
			trace(t, s, key);
			free(key);
			free(rkey);
			continue;
		}
	}
	putchar('\n');
	if(ferror(stdin))
		die("read");
	size_t size, leaves;
	Tsize(t, &size, &leaves);
	printf("SIZE %zu %zu %f %.1f\n",
	       size, leaves,
	       (double)size / leaves,
	       (double)size / leaves / 8);
	const char *key = NULL;
	void *val = NULL, *prev = NULL;
	while(Tnext(t, &key, &val)) {
		assert(key == val);
		printf("< %s\n", key);
//		Tdump(t);
		if(prev) {
			t = Tdel(t, prev);
			free(prev);
		}
		prev = val;
	}
//	Tdump(t);
	t = Tdel(t, prev);
	free(prev);
//	Tdump(t);
	return(0);
}
