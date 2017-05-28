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

In a 4-bit qp trie, we can steal 16 bits from the nybble offset, so a
64 bit index word contains two 16 bit fields for bitmaps, and a 32 bit
nybble offset.

In a 5-bit qp trie, there isn't space in a 64 bit word for all three
fields, so we have to spill into another word.

With the old layout, a 6-bit qp trie was not an attractive option
since it wastes a word per leaf, but that is no longer a problem with
this new layout.

The following table shows how branches can fit reasonably nicely on
the two common word sizes and the three sensible nybble sizes. We want
to keep a branch object to a whole number of words so an array of
branches can be packed tightly.

    nybble
    size        word size       32          64

    4 bit       pointer         32          64
                offset          31+1        31+1
                bitmaps         16 x 2      16 x 2

                words           3           2

    5 bit       pointer         32          64
                offset          29+3        61+3
                bitmaps         32 x 2      32 x 2

                words           4           3

    6 bit       pointer         32          64
                offset          30+2        62+2
                bitmaps         64 x 2      64 x 2

                words           6           4

It's possible to reduce the size of branches by reducing the size of
the offset field (the pointer and bitmap sizes are fixed) but to get
the benefit of smaller offsets we would need to reorganize the branch
array into separate arrays so that small offsets can be packed
tightly. However this is likely to make array indexing more expensive.


Concatenated nodes
------------------

This new layout works with concatenated branch nodes. There is no
longer any need for a branch nybble field. If there is a single bit
set in the branch bitmap, the branch array just contains one offset
and a pair of bitmaps, and instead of a pointer, the child branch's
arrays follow consecutively in memory.


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

When searching, if there is a leaf at a node, compare keys. If they
match, you have succeeded. If the leaf is not a prefix of the search
key we have found a subtrie where we cannot match, so quit. Else check
for a branch; if there is a branch, continue down the trie, or if not,
the search key is not in the trie, so quit.


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


Caveats
-------

The risk of completely user-defined leaf types that embed both key and
value is that the user must take care not to alter the key, otherwise
they will corrupt the trie. I don't know of any way to get the
compiler to help enforce this constraint, and also allow in-place
mutation of the value part.

It's also mildly awkward from the syntax point of view. When the key
and value are the same object, a sugary

        trie[key] = value;

syntax doesn't work. Instead it has to be more like

        trie.insert(leaf);


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
