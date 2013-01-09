// Adaptive critbit tree

typedef uintptr_t act_index; // bit indices
typedef unsigned char byte;

typedef enum act_type {
  act_t_mask = 3,
  act_t_n0 = 0,
  act_t_n1 = 1,
  act_t_n2 = 2,
  act_t_n4 = 3,
} act_type;

typedef union act_ptr {
  uintptr_t t;
  struct act_n0 *n0;
  struct act_n1 *n1;
  struct act_n2 *n2;
  struct act_n4 *n4;
} act_ptr;

struct act_n0 {
  act_index len;
  void *val;
  byte key[1];
};

struct act_n1 {
  act_index i;
  act_ptr sub[2];
};

struct act_n2 {
  act_index i;
  act_ptr sub[4];
};

struct act_n4 {
  act_index i;
  act_ptr sub[16];
};

typedef struct act {
  act_ptr top;
} act;

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
