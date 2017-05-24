Bitstrings and prefixes in a [qp trie](https://dotat.at/prog/qp)
======================================

_May 2017_

[Marek asked](https://twitter.com/vavrusam/status/867070309785980928)

> any thoughts on making qptrie work for bitstrings with length not
> aligned on nibble boundary?

> I want to try it for v6 prefixes, a writeup would be appreciated!
> I'm still not sure how to best store mask for the trailing bits


Original qp trie
----------------

The existing implementation is based on nul-terminated strings, and it
treats the nonexistent bytes after the end of the key as if they are
zero.

The qp trie lookup code fetches a nybble from the search key and turns
it into a bitmap with one bit set. In the code there is the line

        if(offset >= len) return(1);

which is the part that causes bytes after the end of the key to be
treated as zero - 1 is the bitmap with bit zero set.

In the 5 bit and 6 bit variants, it is common for the quintet / sextet
nybbles to be unaligned wrt the end of the key, but that's not a
problem - the bits in the nybble past the end of the key are filled
with zeroes from the nul terminator.


Double bitmap qp trie
---------------------

The (as yet unimplemented) [double bitmap qp trie
layout](notes-generic-leaves.md) allows generic leaf types
embedded in the data structure, and allows arbitrary binary keys.

What happens when the end of a key is unaligned wrt the 5 bit or 6 bit
nybbles?

For now, I'll just consider byte string keys - I'll generalise to
bitstrings later.

In this setting the equivalent to the line quoted above should be

        if(offset >= len) return(0);

That is, an empty bitmap, signifying that there is nothing present in
the key at this offset. When this bitmap is tested against the bitmaps
in a branch, no match will be found - correctly, because we have
descended into a part of the trie where all the keys are longer than
the search key and therefore can't possibly match.

(This tweak would also be valid in the existing implementations, since
a key in the trie has to diverge from its neighbours no later than its
end, so if our offset is after the end of the search key, the key
can't be present since we should already have found it.)

But what happens at the end of a key, when the last byte isn't aligned
with the nybbles? Some of the bits in the nybble are logically not
present, but we can't represent that, at least not in a way that
produces an unambiguous bitmap.

The solution is to observe that there can be at most one byte boundary
in a nybble, so a key that ends in this nybble can't collide in the
leaf bitmap with a longer key.

This means we can fill in the missing bits in the nybble that fall
after the end of the key with zero, and we won't get confused with a
longer key which happens to have zero bits at that point.


Bitstring keys
--------------

OK so far. But if we allow keys to be arbitrary bit strings, then
multiple keys can end inside the same nybble, and we could have
multiple keys trying to occupy the same bit in the leaf bitmap.

However, notice that Marek changed the problem that we are trying to
solve: he asked about IP address prefix matching, which is a search
for the longest key in the trie that matches a prefix of the search
key, not a search for an exact match.

There is a lot of literature for IP address prefix matching, and some
of it describes data structures that are very similar to a qp trie. [I
have previously reviewed a few papers on this
topic.](blog-2016-02-23.md)

But anyway, how would I go about doing this in a qp style?


Example
-------

I'll write bit strings in binary with the big end on the left, and use
5 bit serch keys.

We'll have a shorter prefix, 01/2, with a more specific route for the
subnet 0101/4, and a third route to 101/3.

So keys 01010 and 01011 (ten and eleven) match the longer prefix.

Keys 01000 and 01001 match the shorter prefix, as do keys 01100 up to
01111 (eight, nine, twelve - fifteen).

Keys 10100 up to 10111 (twenty - twenty-three) match the third prefix.

Other keys do not match.

Using S for the short prefix, M for the more specific route, T for the
third prefix, and 0 for no match, the result for each of the 32
possible search keys is:

        00000000 SSMMSSSS 0000TTTT 00000000

Letters correspond to bits set in the leaf bitmap. The leaf array then
looks like

        SSMMSSSSTTTT

Unlike an exact-match qp trie, there are multiple entries for the same
leaf when it can match multiple search keys. This is a bit redundant.
It implies that the leaves probably need to be represented as pointers
rather than being embedded in the trie. Perhaps these overheads are
acceptable.


Longest-prefix search
---------------------

The search algorithm's bitmap handling is the same as an original qp
trie. The differences are in key matching and exit conditions.

When searching, we need to keep track of the longest match found so
far, which obviously starts off NULL.

If there is a leaf at a node, compare the search key with the leaf's
prefix. If they don't match, we have found a subtrie where our search
key cannot match, so return the longest match found so far. If they do
match, this leaf is now our longest match. Keep going.

Next check for a branch; if there is a branch, continue down the trie,
or if not, return the longest match found so far.


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
