Notes on qp trie rib compression
================================

Since January I have been thinking on and off about the details of
what started as @tef's write buffer suggestion.


Terminology
-----------

There are two kinds of compression in the original version of qp tries:

* Spine compression, from Morrison's PATRICIA tries, in which
  sequences of branches with one child each are omitted, and instead
  of key indexes being implicit in the tree depth, each node is
  annotated with an explicit key index.

* Branch compression, using Bagwell's HAMT popcount trick, in which
  null pointers to missing child nodes are omitted, and there is a
  bitmap indicating which child nodes are present.

The new idea will add a third kind of compression, which I will call
"rib compression", in which a branch where all but one child is a leaf
is concatenated with its child branch, to save a pointer indirection.


Index word layout
-----------------

My starting point is the quintuple-bit qp trie version, where the
nybbles are 5 bits and the bitmap is 32 bits.

There will be two kinds of index words, for branches (similar to
before) and for ribs.

A rib index needs to identify which nybble values have leaf children,
which nybble value is the branch child, and which are omitted. There
are a couple of reasonable layouts:

1. A bitmap for the leaf children, plus a nybble value for the branch
   child, requiring 32 + 5 = 37 bits.

2. A list of nybble values, consisting of N leaf children and 1 branch
   child, plus a count of children. This requires 5*(N+1) + log(N+1)
   bits. If N is 6, this makes 7*5 + 3 = 38 bits.

The parent branch index word also needs something to indicate how many
ribs precede the main branch array.

The branch index needs a flag bit set to distinguish branches from
leaves (tagged pointers). But rib indexes do not need a flag bit
because their type is clear from context.

The flag bit has to be at the little end of the word, so the parts of
the words that vary between branches and ribs have to be at the little
end. So whereas the original qp trie put the octet offset at the
bottom of the word, it now makes more sense to put it at the top of
the word.

Option 1 branch:

    +-------------+-----------+-------------+----------+---+
    | offset (24) | shift (3) | bitmap (32) | ribs (4) | 1 |
    +-------------+-----------+-------------+----------+---+

Option 1 rib:

    +-------------+-----------+-------------+--------------+
    | offset (24) | shift (3) | bitmap (32) |  child  (5)  |
    +-------------+-----------+-------------+--------------+

Option 2 branch:

    +-------------+-----------+-------------+----------+---+
    | offset (23) | shift (3) | bitmap (32) | ribs (5) | 1 |
    +-------------+-----------+-------------+----------+---+

Option 2 rib:

    +-------------+-----------+----------------+-----------+
    | offset (23) | shift (3) | children (5*7) | count (3) |
    +-------------+-----------+----------------+-----------+


Rib selection
-------------

In both cases the rib index word is followed by N leaves (key/value
pairs).

For option 1, the leaves can be indexed using the popcount trick. When
the nybble's bit is not set, check the child field for a match, and
either quit or continue searching.

For option 2, we can iterate through the children nybbles, or we can use
[Mycroft's SWAR strlen() trick](https://www.cl.cam.ac.uk/~am21/progtricks.html)
combined with CLZ to work out if a child matches.


Discussion
----------

I was stuck on choosing a good memory layout for a long time, I think
because I couldn't find any clear constraints to guide the choice of
field sizes - especially for the rib count.

When I thought of sawpping the order of the octet offset / byte shift
/ bitmap fields, it became a lot more clear how to make good use of
the available space.

For a while I was planning to use Mycroft's strlen() trick, because it
is enormously fun. But after working through the layouts in more
detail it looks like option 1 will be better.


Other fan-out widths
--------------------

Option 1 also works well in a 4-bit qp trie:

4-bit option 1 branch:

    +-------------+-----------+-------------+----------+---+
    | offset (43) | shift (1) | bitmap (16) | ribs (3) | 1 |
    +-------------+-----------+-------------+----------+---+

4-bit option 1 rib:

    +-------------+-----------+-------------+--------------+
    | offset (43) | shift (1) | bitmap (16) |  child  (4)  |
    +-------------+-----------+-------------+--------------+


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

But does the knot not bit work for rib compression?

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
