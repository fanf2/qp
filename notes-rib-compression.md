Notes on qp trie rib compression
================================

Since January I have been thinking on and off about the details of
what started as @tef's write buffer suggestion. It has taken a long
time to distill down to something reasonably simple...


A third kind of compression
---------------------------

There are two kinds of compression in the original version of qp tries:

* Spine compression, from Morrison's PATRICIA tries, in which
  sequences of branches with one child each are omitted, and instead
  of key indexes being implicit in the tree depth, each node is
  annotated with an explicit key index.

* Branch compression, using Bagwell's HAMT popcount trick, in which
  null pointers to missing child nodes are omitted, and there is a
  bitmap indicating which child nodes are present.

The new idea will add a third kind of compression, which I will call
"rib compression", in which a branch that has leaves for all its
children except for one child branch, is concatenated with its child
branch to save a pointer indirection.

(The term "rib compression" was inspired by "spine compression". The
idea is that a linear sequence of nodes with leaves sprouting off to
the sides is a bit like a rib cage.)


Rib branches
------------

A "rib" branch needs to identify which nybble values have leaf
children, and which nybble value is the branch child. We want to move
the branch out of the twig array, so that we aren't wasting a word on
the unused branch pointer. This implies that it should not be in the
bitmap; instead there needs to be a field containing the nybble value
of the branch child.

(Actually, it's probably possible to omit this extra field, since we
can recover its value from a stored child key when updating the trie.
But the disadvantage is that we lose the ability to stop early when
looking for a missing key, and we need some other way to identify rib
branches.)

The single branch child of a rib branch is concatenated onto its
parent. Instead of following a pointer, the child appears
consecutively in memory after its parent's twig array.


Indirect and concatenated branches
----------------------------------

In an original qp trie, all branches have an indirect layout. They
have two parts:

* a twig containing an index word and a pointer;

* an array of child twigs.

With rib compression, the child branch of a rib branch has a
concatenated layout:

* a bare index word, consecutively followed by

* an array of child twigs.


Trunks
------

We'll call a consecutive sequence of concatenated branches a "trunk".
A trunk corresponds to a single allocation.

Every branch in a trunk except for the last must be a rib branch, i.e.
must have exactly one branch child, the rest being leaves.

The first branch in a trunk has an indirect layout - the pointer is
how we find the trunk. All the rest are concatenated.

A branch (of any shape) is concatenated to its parent exactly when its
parent is a rib branch.


Index word layout
-----------------

On a little-endian machine the layout of an index word for a
quintuple-bit qp trie with rib compression is:

        uint64_t    tag    : 1,
                    shift  : 3,
                    offset : 23,
                    branch : 5,
                    bitmap : 32;

For a quadruple-bit qp trie it is:

        uint64_t    tag    : 1,
                    shift  : 1,
                    offset : 42,
                    branch : 4,
                    bitmap : 16;

The branch field is only used in the index word of a rib branch, and
it refers to the child node concatenated after the rib node. In this
case the bit corresponding to the branch field is clear in the bitmap.

To distinguish a rib branch from another kind of branch from looking
at its index, the concatenated child branch field should correspond
to one of the bits set in the bitmap.

When looking up a key in the qp trie, the lookup code first checks the
bitmap for a match (in which case the child is found in the twig
array), then checks the branch field for a match (in which case the
child is concatenated), otherwise the key is missing.


Binary keys
-----------

The original qp trie only supports nul-terminated C string keys, and
treats all bytes after the end of the key as zero.

I have considered a hack to make it work for arbitrary binary keys, by
treating the first byte after the end to be one and subsequent bytes
zero. This works for relaxing the prefix-freedom requirement, but it
breaks lexical ordering: a shorter string should be lexically before
any longer string of which it is a prefix, but with the hack a longer
string that continues with zero bytes will be before the shorter
string.

In the Knot DNS qp trie implementation, there is an extra bit in the
bitmap to indicate a non-octet, which sorts before a zero octet. This
is probably a better solution than my hack.

But does the Knot not bit work for rib compression?

There is a potential problem in the rib index word where there is a
copy of the child nybble corresponding to the following branch. There
isn't space here to represent a non-octet value (without stealing yet
another bit).

But any time a non-octet appears in an index, it must be a leaf, so in
a rib it will appear in the bitmap of leaves not the child branch
slot. So the problem never occurs.

The reason a non-octet must be a leaf is that all the subsequent
octets in this key must also be non-octets, and there is only one key
like this.

This logic works for keys which are a single binary string, but it
doesn't work for keys like DNS names which are sequences of binary
strings separated by non-octets. In this setting it seems reasonable
to promote troublesome ribs to branches, when they have non-octet
following branches.


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <http://dotat.at/>;
You may do anything with this. It has no warranty.
<http://creativecommons.org/publicdomain/zero/1.0/>
