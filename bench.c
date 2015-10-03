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
"usage: %s <seed> <input>\n"
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
	printf("%ld.%06d s\n", tv.tv_sec, tv.tv_usec);
}

static int
ssrandom(char *s) {
	// initialize random(3) from a string
	size_t len = strlen(s);
	if(len < 12) return(-1);
	unsigned char *u = (unsigned char *)s;
	unsigned seed = u[0] | u[1] << 8 | u[2] << 16 | u[3] << 24;
	initstate(seed, s+4, len-4);
	return(0);
}

int
main(int argc, char *argv[]) {
	progname = argv[0];
	if(argc != 3 || argv[1][0] == '-') usage();
	if(ssrandom(argv[1]) < 0) usage();

	start("reading");
	int fd = open(argv[2], O_RDONLY);
	if(fd < 0) die("open");
	struct stat st;
	if(fstat(fd, &st) < 0) die("stat");
	size_t flen = (size_t)st.st_size;
	char *t = malloc(flen + 1);
	if(t == NULL) die("malloc");
	if(read(fd, t, flen) < 0) die("read");
	close(fd);
	t[flen] = '\0';
	done();

	start("scanning");
	size_t lines = 1;
	for(char *p = t; *p; p++)
		if(*p == '\n')
			++lines;
	const char **line = calloc(lines, sizeof(*line));
	int l = 0;
	bool start = true;
	for(char *p = t; *p; p++) {
		if(start) {
			line[l++] = p;
			start = false;
		}
		if(*p == '\n') {
			*p = '\0';
			start = true;
		}
	}
	done();
	printf("got %zu lines\n", lines);
}
