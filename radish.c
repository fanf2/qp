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
