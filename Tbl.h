// Tbl.h: an abstract API for tables with string keys.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#ifndef Tbl_h
#define Tbl_h

// A table is represented by a pointer to this incomplete struct type.
// You initialize an empty table by setting the pointer to NULL.
//
typedef struct Tbl Tbl;

// Get the value associated with a key.
// Returns NULL if the key is not in the Table.
//
void *Tgetl(Tbl *tbl, const char *key, size_t klen);
void *Tget(Tbl *tbl, const char *key);

// Returns false if the key is not found, otherwise returns true and
// sets *rkey and *rval to the table's key and value pointers.
//
bool Tgetkv(Tbl *tbl, const char *key, size_t klen, const char **rkey, void **rval);

// Associate a key with a value in a table. Returns a new pointer to
// the modified table. If there is an error it sets errno and returns
// NULL. To delete a key, set its value to NULL. When the last key is
// deleted, Tset() returns NULL without setting errno. The key and
// value are borrowed not copied.
//
// Errors:
// EINVAL - value pointer is not word-aligned
// ENOMEM - allocation failed
//
Tbl *Tsetl(Tbl *tbl, const char *key, size_t klen, void *value);
Tbl *Tset(Tbl *tbl, const char *key, void *value);
Tbl *Tdell(Tbl *tbl, const char *key, size_t klen);
Tbl *Tdel(Tbl *tbl, const char *key);

// Deletes an entry from the table as above, and sets *rkey and *rval
// to the removed key and value pointers.
//
Tbl *Tdelkv(Tbl *tbl, const char *key, size_t klen, const char **rkey, void **rval);

// Find the next item in the table. The p... arguments are in/out
// parameters. To find the first key, pass *pkey=NULL and *pklen=0.
// For subsequent keys, *pkey must be present in the table and is
// updated to the lexicographically following key. Returns false or
// NULL when there are no more keys.
//
bool Tnextl(Tbl *tbl, const char **pkey, size_t *pklen, void **pvalue);
bool Tnext(Tbl *tbl, const char **pkey, void **pvalue);
const char *Tnxt(Tbl *tbl, const char *key);

// Debugging
//
void Tdump(Tbl *tbl);
void Tsize(Tbl *tbl, const char **rtype,
    size_t *rsize, size_t *rdepth, size_t *rbranches, size_t *rleaves);

#endif // Tbl_h
