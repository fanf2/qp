// bench.c: table benchmark.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>
#include <unistd.h>

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
"usage: %s <seed> <count> <input>\n"
"	The seed must be at least 12 characters.\n"
		, progname);
	exit(1);
}

static struct timeval tu;

static void
start(const char *s) {
	printf("%s... ", s);
	gettimeofday(&tu, NULL);
}

static void
done(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tv.tv_sec -= tu.tv_sec;
	tv.tv_usec -= tu.tv_usec;
	if(tv.tv_usec < 0) {
		tv.tv_sec -= 1;
		tv.tv_usec += 1000000;
	}
	printf("%ld.%06ld s\n",
	       (long)tv.tv_sec, (long)tv.tv_usec);
}

static int
ssrandom(char *s) {
	// initialize random(3) from a string
	size_t len = strlen(s);
	if(len < 12) return(-1);
	unsigned seed = s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
	initstate(seed, s+4, len-4);
	return(0);
}

int
main(int argc, char *argv[]) {
	progname = argv[0];
	if(argc != 4 || argv[1][0] == '-') usage();
	if(ssrandom(argv[1]) < 0) usage();
	uint N = (uint)atoi(argv[2]);

	int fd = open(argv[3], O_RDONLY);
	if(fd < 0) die("open");
	struct stat st;
	if(fstat(fd, &st) < 0) die("stat");
	size_t flen = (size_t)st.st_size;
	char *fbuf = malloc(flen + 1);
	if(fbuf == NULL) die("malloc");
	if(read(fd, fbuf, flen) < 0) die("read");
	close(fd);
	fbuf[flen] = '\0';

	size_t lines = 0;
	for(char *p = fbuf; *p; p++)
		if(*p == '\n')
			++lines;
	char **line = calloc(lines, sizeof(*line));
	size_t l = 0;
	bool bol = true;
	for(char *p = fbuf; *p; p++) {
		if(bol) {
			line[l++] = p;
			bol = false;
		}
		if(*p == '\n') {
			*p = '\0';
			bol = true;
		}
	}
	printf("- got %zu lines\n", lines);

	start("load");
	Tbl *t = NULL;
	for(l = 0; l < lines; l++)
		t = Tset(t, line[l], main);
	done();

	start("search");
	l = 0;
	for(int i = 0; i < N; i++)
		if(Tget(t, line[random() % lines]) != NULL)
			++l;
	assert(l == N);
	done();

	start("mutate");
	for(int i = 0; i < N; i++)
		t = Tset(t, line[random() % lines],
			 random() % 2 ? main : NULL);
	done();

	// ensure all keys present
	for(l = 0; l < lines; l++)
		t = Tset(t, line[l], main);
	start("free");
	for(l = 0; l < lines; l++)
		t = Tset(t, line[l], NULL);
	assert(t == NULL);
	done();

	return(0);
}
