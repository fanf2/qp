// Tbl.c: simpler wrappers for core table functions
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "Tbl.h"

void *
Tgetl(Tbl *tbl, const char *key, size_t len) {
	const char *rkey = NULL;
	void *rval = NULL;
	if(Tgetkv(tbl, key, len, &rkey, &rval))
		return(rval);
	else
		return(NULL);
}

void *
Tget(Tbl *tbl, const char *key) {
	return(Tgetl(tbl, key, strlen(key)));
}

Tbl *
Tset(Tbl *tbl, const char *key, void *value) {
	return(Tsetl(tbl, key, strlen(key), value));
}

Tbl *
Tdell(Tbl *tbl, const char *key, size_t len) {
	const char *rkey = NULL;
	void *rval = NULL;
	return(Tdelkv(tbl, key, len, &rkey, &rval));
}

Tbl *
Tdel(Tbl *tbl, const char *key) {
	return(Tdell(tbl, key, strlen(key)));
}

bool
Tnext(Tbl *tbl, const char **pkey, void **pvalue) {
	size_t len = *pkey == NULL ? 0 : strlen(*pkey);
	return(Tnextl(tbl, pkey, &len, pvalue));
}

const char *
Tnxt(Tbl *tbl, const char *key) {
	void *value = NULL;
	Tnext(tbl, &key, &value);
	return(key);
}
