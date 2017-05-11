Generic leaf types in a [qp trie](https://dotat.at/prog/qp)
=================================

_May 2017_


The original qp trie is based around a two-word "twig" object, which
can be either a leaf (a key+value pair of pointers) or a branch (an
index word and a pointer).

When I benchmarked memory overhead, I took the key+value pair as a
sunk cost. But it's common (especially in the C++ world) to want to
embed the key and value rather than reference them indirectly.
Similarly, DJB's crit-bit trie has single-word leaves that just point
to the key; if you want to store a key+value pair, you need to embed
the key in the value so that you can find the value given the key.

These efficiency tricks don't work in a qp trie because the layout of
leaves is tied to the layout of branches. Can we decouple them, to
make the layout of leaves more flexible and efficient?


Old: one array, one bitmap
--------------------------

The original branch layout consists of:

* an index word, which contains the offset into the key of the
  branch's controlling nybble, and a bitmap indicating which child
  nodes are present;

* a pointer to an array of child "twigs", each of which can be either
  a leaf or a branch.


New: two arrays, two bitmaps
----------------------------

The new layout segregates child nodes into separate arrays of branches
and leaves. Each array has its own bitmap, and the bitmaps must have
an empty intersection.

In effect, the tag bits inside twigs (the flags field that was used to
distinguish between leaves and branches) have been moved up into the
index word.

As before, each element in the branch array consists of an index word
and a pointer. The child's two arrays are placed consecutively in
memory at the target of the pointer, so only one pointer is needed.

The type of elements of the leaf array can be entirely under the
control of the user.


Making space
------------

We need to find space for this second bitmap.

In a 4-bit qp trie, we can steal 16 bits from the nybble offset, so
the index word contains two 16 bit fields for bitmaps, and a 32 bit
nybble offset.

In a 5-bit qp trie, there isn't space in a 64 bit word for all three
fields, but we have enough flexibility to use a packed array of 3x32
bit words.

With the old layout, a 6-bit qp trie was not an attractive option
since it wastes a word per leaf, but that is no longer a problem with
this new layout.

To pack the branch array as effectively as possible, it might be
helpful in some cases to split it into two or three separate arrays
(of pointers, bitmaps, and offsets) so that smaller fields don't mess
up the alignment requirements of larger fields. This can make it
possible to save more space by limiting offsets to (say) 16 bits.
However splitting the arrays is likely to make array indexing more
expensive.


Concatenated nodes
------------------

This new layout works with concatenated branch nodes. There is no
longer any need for a branch nybble field. If there is a single bit
set in the branch bitmap, the branch array just contains an index
word, and instead of a pointer, the child branch's arrays follow
consecutively in memory.


Binary keys and prefix agnosticism
----------------------------------

Two observations:

* To support binary keys as described at the end of the [notes on rib
  compression](notes-rib-compression.md), the leaf bitmap needs an
  extra bit. This is annoying with wide fanouts, because the bitmaps
  no longer fit in a word.

* The two bitmaps are somewhat redundant: zero in both means no nodes
  with this prefix; a one and a zero means either a leaf or a branch;
  but two ones doesn't have an assigned meaning.

Having both a leaf and a branch at the same point in the trie implies
that we have relaxed the requirement for prefix-freedom. This
relaxation also means we no longer have a problem with binary keys, so
we don't need the extra valueless bit in the leaf bitmap.

When a child has bits set in both bitmaps, this means that the the
leaf key is longer than the offset of this nybble, but shorter than
the offsets of all children in the branch. In other words, a leaf is
pushed down the tree as far as possible.

When searching, if there is a leaf at a node, compare keys; if they
match, you have succeeded, else you need to check for a branch; if
there is a branch, continue down the trie, or if not, the search key
is not in the trie.


Portability and genericity
--------------------------

The new layout is overall a lot more type-safe, since different types
of object are placed in different parts of memory, rather than being
distinguished by tag bits.

This greatly reduces portability problems due to type punning between
the index word and a pointer - things like endianness and word size
mismatches can mess up the placement of the tag bit.

The lack of coupling allows leaf type to be completely generic, and
the genericity could be straightforwardly extended to key comparisons
and fetching nybbles.

The main requirement on leaves is that they can be moved around
freely, when arrays are resized to insert or delete child nodes.

Overall, this new layout should be a lot more friendly to C++ and Rust.


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
