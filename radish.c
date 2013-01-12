// Adaptive critbit tree

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

typedef uintptr_t acbt_i; // bit indices
typedef unsigned char byte;

// Node pointers
//
// The bottom bits determine which type of node is the target of the pointer.
//
typedef union acbt_ptr {
  uintptr_t t;
  struct acbt_n0 *n0;
  struct acbt_n1 *n1;
  struct acbt_n2 *n2;
  struct acbt_n4 *n4;
} acbt_ptr;

enum {
  acbt_t_mask = 3,
  acbt_t_n0 = 0,
  acbt_t_n1 = 1,
  acbt_t_n2 = 2,
  acbt_t_n4 = 3,
};

static unsigned acbt_type(acbt_ptr p) {
  return(p.t & acbt_t_mask);
}

typedef struct acbt {
  union acbt_ptr top;
} acbt;

// Leaf nodes
//
// 'len' is the length of the key in bits. The key is implicitly
// followed by a one bit then an infinite string of zero bits. These
// implicit bits are not stored. The one bit ensures that keys of
// different lengths compare differently.
//
typedef struct acbt_n0 {
  void *val;
  acbt_i len;
  byte key[];
} acbt_n0;

// Single bit nodes
//
// If the key is present in the tree it will be in the subtree
// p.n1->sub[acbt_i1(key, len, p.n1->i)]
//
typedef struct acbt_n1 {
  acbt_i i;
  acbt_ptr sub[2];
} acbt_n1;

// Double bit nodes
//
// The index must be a multiple of two.
//
// If the key is present in the tree it will be in the subtree
// p.n2->sub[acbt_i2(key, len, p.n2->i)]
//
typedef struct acbt_n2 {
  acbt_i i;
  acbt_ptr sub[4];
} acbt_n2;

// Quad bit nodes
//
// The index must be a multiple of four.
//
// If the key is present in the tree it will be in the subtree
// p.n4->sub[acbt_i4(key, len, p.n4->i)]
//
typedef struct acbt_n4 {
  acbt_i i;
  acbt_ptr sub[16];
} acbt_n4;

// What is the memory cost (counted in pointer-sized words) excluding
// the copies of the keys and the value pointers? Each leaf includes
// the length of the key, which is arguably overhead but cannot easily
// be omitted, so we will not count it. The total size of the interior
// and root nodes is the interesting sum.
//
// In a simple one-at-a-time critbit tree, the overhead is 3*N-2,
// since there are N-1 interior nodes.
//
// The idea for the adaptive critbit tree is to save space and lookup
// time by coalescing nodes that have adjacent critical bits.
//
// Adjacent n1 nodes may cover three or four subtrees (depending on
// whether both of the child n1 nodes are present or not) with a cost
// of 6 or 9 words. An n2 node costs 5 words, so it is always a win to
// coalesce. If one of the child nodes is absent then its pointer in
// the parent gets duplicated.
//
// Adjacent n2 nodes are more complicated. The parent may have three
// or four children. Each child may be an adjacent n2, an adjacent n1,
// an off-by-one n1 (which must also be coalesced), or something
// further away. When coalescing we delete all the child nodes and the
// parent 5-word node and replace them with a 17-word node. So any
// benefit amounts to the total size of the child nodes minus 12.
//
// 0*n1+0*n2 -> -12  0
// 1*n1+0*n2 -> -9   1
// 2*n1+0*n2 -> -6   2
// 3*n1+0*n2 -> -3   3
// 4*n1+0*n2 ->  0   4
// 0*n1+1*n2 -> -7   2
// 1*n1+1*n2 -> -4   3
// 2*n1+1*n2 -> -1   4
// 3*n1+1*n2 -> +2   5
// 0*n1+2*n2 -> -2   4
// 1*n1+2*n2 -> +1   5
// 2*n1+2*n2 -> +4   6
// 0*n1+3*n2 -> +3   6
// 1*n1+3*n2 -> +6   7
// 0*n1+4*n2 -> +8   8
//
// So the sum of the type codes of the coalescable child nodes must be
// at least 5, and at least one of them must be an n2.


// Two versions of count leading zeroes

#ifdef __GNUC__

static acbt_i acbt_clz(unsigned b) {
  return(b & 0xFF ? __builtin_clz(b) - __builtin_clz(0xFF) : 8);
}

#else

static acbt_i acbt_clz(unsigned b) {
  acbt_i i = 0;
  if(b & 0xF0) b &= 0xF0; else i += 4;
  if(b & 0xCC) b &= 0xCC; else i += 2;
  if(b & 0xAA) b &= 0xAA; else i += 1;
  return(b & 0xFF ? i : 8);
}

#endif

// Extract the key byte in which the bit index falls.
// The result includes the implicit trailing bits.
static inline byte acbt_i8(byte *key, acbt_i len, acbt_i i) {
  acbt_i trail = len % 8;
  acbt_i blen = len / 8;
  acbt_i bi = i / 8;
  if(bi < blen)
    return(key[bi]);
  if(bi > blen)
    return(0);
  if(trail == 0)
    return(0x80);
  else
    return((key[bi] & (0xFF00 >> trail)) | (0x80 >> trail));
}

// Extract a single bit from a key.
static byte acbt_i1(byte *key, acbt_i len, acbt_i i) {
  return((acbt_i8(key, len, i) >> 7 - (i & 7)) & 0x01);
}

// Extract a double bit from a key.
static byte acbt_i2(byte *key, acbt_i len, acbt_i i) {
  return((acbt_i8(key, len, i) >> 6 - (i & 6)) & 0x03);
}

// Extract a quad bit from a key.
static byte acbt_i4(byte *key, acbt_i len, acbt_i i) {
  return((acbt_i8(key, len, i) >> 4 - (i & 4)) & 0x0F);
}

// Find the leaf that is most similar to this key.
static acbt_n0 *acbt_walk(acbt_ptr p, byte *key, acbt_i len) {
  for(;;) {
    switch(acbt_type(p)) {
    case(acbt_t_n0):
      return(p.n0);
    case(acbt_t_n1):
      p = p.n1->sub[acbt_i1(key, len, p.n1->i)];
      continue;
    case(acbt_t_n2):
      p = p.n2->sub[acbt_i2(key, len, p.n2->i)];
      continue;
    case(acbt_t_n4):
      p = p.n4->sub[acbt_i4(key, len, p.n4->i)];
      continue;
    default:
      abort();
    }
  }
}

static const acbt_i acbt_eq = ~(acbt_i)0;

// Return the index of the critical bit if the keys differ,
// or acbt_eq if they are the same.
static acbt_i acbt_cb(byte *k1, acbt_i l1, byte *k2, acbt_i l2) {
  acbt_i i, l;
  l = l1 > l2 ? l1 : l2;
  for(i = 0; i <= l; i += 8) {
    byte b = acbt_i8(k1, l1, i) ^ acbt_i8(k2, l2, i);
    if(b) return(i + acbt_clz(b));
  }
  return(acbt_eq);
}

static acbt_ptr acbt_new0(byte *key, acbt_i len, void *val) {
  acbt_i blen = (len + 7) / 8;
  acbt_ptr p;
  p.n0 = malloc(sizeof(*p.n0) + blen);
  memcpy(p.n0->key, key, blen);
  p.n0->len = len;
  p.n0->val = val;
  return(p);
}

static void *acbt_first(acbt *t, byte *key, acbt_i len, void *val) {
  if(val != NULL)
    t->top = acbt_new0(key, len, val);
  return(val);
}

static void *acbt_delete(acbt *t, byte *key, acbt_i len) {
}

static acbt_n0 *acbt_insert(acbt *t, acbt_i cb, byte *key, acbt_i len, void *val) {
}

static acbt_n0 *acbt_find(acbt *t, byte *key, acbt_i len, void *val) {
  acbt_n0 *n0 = acbt_walk(t->top, key, len);
  acbt_i cb = acbt_cb(n0->key, n0->len, key, len);
  if(cb == acbt_eq)
    return(n0);
  else
    return(acbt_insert(t, cb, key, len, val));
}

void *acbt_alter(acbt *t, void *key, acbt_i len, void *val) {
  if(t->top.n0 == NULL)
    return(acbt_first(t, key, len, val));
  if(val == NULL)
    return(acbt_delete(t, key, len));
  acbt_n0 *n0 = acbt_find(t, key, len, val);
  void *old = n0->val;
  n0->val = val;
  return(old);
}

void *acbt_query(acbt *t, void *key, acbt_i len, void *val) {
  if(t->top.n0 == NULL)
    return(acbt_first(t, key, len, val));
  else
    return(acbt_find(t, key, len, val)->val);
}
