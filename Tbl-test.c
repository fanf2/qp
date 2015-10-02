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
	void *val;
	printf("%c%s", s, key);
	key = val = NULL;
	while(Tnext(t, &key, &val))
		printf("* %s", key);
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
		if(s < 0)
			break;
		if(getline(&key, &len, stdin) < 0)
			break;
		switch(s) {
		default:
			usage();
		case('+'):
			errno = 0;
			void *val = Tget(t, key);
			if(val == NULL)
				val = key;
			t = Tsetl(t, key, len, val);
			if(t == NULL)
				die("Tbl");
//			trace(t, s, key);
			if(val != key)
				free((void*)key);
			continue;
		case('-'):
			errno = 0;
			const char *rkey = NULL;
			void *rval = NULL;
			t = Tdelkv(t, key, len, &rkey, &rval);
			if(t == NULL && errno != 0)
				die("Tbl");
//			trace(t, s, key);
			free((void*)key);
			free((void*)rkey);
			continue;
		}
	}
	if(ferror(stdin))
		die("read");
	const char *key = NULL;
	void *val = NULL, *prev = NULL;
	while(Tnext(t, &key, &val)) {
		assert(key == val);
		fputs(key, stdout);
		t = Tdel(t, key);
		free((void*)prev);
		prev = val;
	}
	free((void*)prev);
	return(0);
}
