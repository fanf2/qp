// A generic tree API.

// An empty tree is a NULL pointer
typedef struct Tree Tree;

// Get the value associated with a key.
// Returns NULL if the key is not in the tree.
//
void *Tget(Tree *tree, const char *key);

// Associate a key with a value in a tree.
// Value pointer must be word-aligned.
// To delete a key, set its value to NULL.
// Key and value are borrowed not copied.
//
Tree *Tset(Tree *tree, const char *key, void *value);

// Find the next key in the tree.
// To find the first key, pass key = NULL.
// Returns NULL when there are no more keys.
//
const char *Tnext(Tree *tree, const char *key);
