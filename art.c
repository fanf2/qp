/*
 * adaptive radix tree
 */

typedef unsigned char byte;

static inline void *wordup(void *p) {
	static const uintptr_t mask = sizeof(void*) - 1;
	return(void*)((uintptr_t)p + mask & ~mask);
}

/*
 * All nodes start with a key index. Nodes nearer the root have lower indices.
 *
 * All nodes have a max value, which is the maximum number of entries in
 * the sub pointer array. The max value also indicates the layout of the
 * node. This value is between 1 and 256; 256 is encoded as zero and 1
 * indicates this is a leaf node. XXX What are the thresholds between the
 * other layouts?
 *
 * Nodes may have a population count, which is the number of non-NULL
 * sub pointers.
 *
 * They may have a high-water mark; all the sub pointers between n->hwm and
 * n->max are NULL.
 *
 * They may have a which array. Each element in the which array n->which[x]
 * has a corresponding element in the sub array artnsub(n)[x]. The keys in
 * this subtree all satisfy key[n->i] == n->which[x].
 */

/* A node's sub array is stored in memory before the node. */
static inline artn **artnsub(artn *n) {
	return (artn **)n - n->max;
}

/* Except for max-size nodes, where the sub array has 256 entries, so we
   add a bit of indirection so two or four of them fit nicely in a page. */
static inline artn **artnsubmax(node *n) {
	return ((artn ***)n)[-1];
}

/*
 * Generic node structure.
 */
typedef struct artn {
	uint32_t i;
	byte max;
} artn;

/*
 * In a leaf node, the value associated with the key is *artnsub(n). The
 * index is the length of the key.
 */
typedef struct artn_leaf {
	uint32_t i;
	byte max;
	byte key[];
} artn_leaf;

/*
 * In a small node, the which array is not sorted. It is searched using
 * SIMD techniques.
 */
typedef struct artn_small {
	uint32_t i;
	byte max;
	byte which[];
} artn_small;

/*
 * In a medium node the which array is sorted so it can be searched
 * reasonably fast without too much memory traffic.
 */
typedef struct artn_medium {
	uint32_t i;
	byte max;
	byte hwm;
	byte pop;
	byte _;
	byte which[];
} artn_medium;

/*
 * Large nodes do not require searching. The subtree index is
 * n->where[key[n->i]] provided this value is less than n->max.
 */
typedef struct artn_large {
	uint32_t i;
	byte max;
	byte hwm;
	byte pop;
	byte _;
	byte where[256];
} artn_large;
