// Adaptive crit-bit tree

#ifndef ACBT_H
#define ACBT_H

// The acbt structure holds an adaptive crit-bit tree root pointer.
// The caller is responsible for allocating and freeing it. The tree
// manipulation functions alter the pointer. You can move a tree's
// root structure around but it should have only one valid root at a
// time.
typedef struct acbt {
  struct acbt_private *p;
} acbt;

// Keys are copied into the tree. The value stored with a key is a
// generic pointer.
//
// Keys are treated as big-endian bit strings and can be an arbitrary
// number of bits long. Bytes are numbered 0, 8, 16, 24, etc. from
// lower to higher memory addresses; bits within a byte are numbered 0
// to 7 from the left most significant end to the right least
// significant end. If the key is not a multiple of 8 bits long then
// the least significant bits of the last byte are ignored.
//
// When comparing keys of different lengths, each key is implicitly
// followed by a one bit then an infinite string of zero bits. These
// implicit bits are not stored. The implicit bits ensure that keys of
// different lengths compare differently. This has the slightly
// strange effect that shorter keys are lexicographically ordered in
// between low-valued and high-valued longer keys. If keys are textual
// strings then the trailing zero byte should be included in the key,
// which preserves normal lexicographic ordering, shorter strings first.
//
typedef uintptr_t acbt_index;

void *acbt_alter(acbt *t, void *key, acbt_index len, void *val);
void *acbt_query(acbt *t, void *key, acbt_index len, void *val);
void acbt_free(acbt *t);
void acbt_move(acbt *dst, acbt *src);

#endif // ACBT_H
