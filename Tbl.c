// Tbl.c: simpler wrappers for core table functions
//
// #include this file in your implementation of the rest of Tbl.h
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

void *
Tget(Tbl *tbl, const char *key) {
	return(Tgetl(tbl, key, strlen(key)));
}

Tbl *
Tset(Tbl *tbl, const char *key, void *value) {
	return(Tsetl(tbl, key, strlen(key), value));
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
