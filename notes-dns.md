Storing DNS host names in a [qp trie](https://dotat.at/prog/qp)
=====================================

A few thoughts on tuning qp tries for storing DNS names, inspired by
Knot DNS. They found that HAT tries were faster than qp tries, mainly
due to requiring fewer memory indirections - HAT tries indirect once
per byte for dense key sets, whereas qp tries indirect twice. Knot is
using the 4 bit version of qp tries; the 5 bit version is unlikely to
be much better in this application.


String theory
--------------

One of the criteria for the design of qp tries was to allow arbitrary
C strings as keys, without undue length restrictions. So as much space
as possible is reserved in the branch word for the key index.

For DNS names, the maximum length is very small, so there's plenty of
space for other purposes.

The usual alphabet for DNS names is case-insensitive letters, digits,
hyphens, sometimes underscore, plus an end-of-label marker which does
not have an octet value. However it is possible to use any octet value
(including zero!) though it is rare.


DNS-trie layout
---------------

There are two kinds of branch nodes, a bit like the upper nybble and
lower nybble of qp-tries, except the split is more cunning.


### upper node

Some of the bits in the upper node bitmap are allocated to whole byte
values. If the byte at the offset is in the usual hostname alphabet, it
is handled by this upper node, with at most one indirection per byte.

  * 26 case-insensitive letters

  * 10 digits

  * hyphen and underscore

  * end-of-label

  * (39 bits so far)

If a byte in the name isn't in the usual alphabet, then it is split
into its upper 3 bits and lower 5 bits. Therefore the upper node
bitmap also contains:

  * 8 for upper 3 bits of non-hostname bytes

  * (47 bits so far)

The rest of the index word has:

  * 2 bit node type

  * mark bit

  * 14 spare bits for the offset


### lower node

If the byte at the offset is a non-hostname value, it can need a second
branch node for its lower 5 bits. This lower node has a more simple
layout:

  * 32 for lower 5 bits of non-hostname bytes

  * end-of-label

  * 2 bit node type

  * mark bit

  * 28 spare bits for the index


### iteration order

The tricky bit of this scheme is iterating the trie in lexical order.
For example, if the upper 3 bits of an octet are 001 (hex values 0x20 -
0x3F) the order of iteration has to switch back and forth between parent
and child node depending on whether the octet is a hyphen or digit or
not.

  * 0x20 - 0x2c: iterate children of child node for upper bits 001

  * 0x2d: iterate child node for `-`

  * 0x2e - 0x2f: iterate children of child node for upper bits 001

  * 0x30 - 0x39: iterate child nodes for `0` - `9`

  * 0x3a - 0x3f: iterate children of child node for upper bits 001

The non-byte end-of-label value has to sort before other values.

We can avoid this complication by breaking up the bitmap for the top 3
bits of non-hostname bytes, so that bits appear in the bitmap in
strictly lexicographic order, which also means that child nodes appear
in lexicographic order. This requires two more bits in the bitmap
because of the way the 0x20 - 0x3f block is broken up (as described
above), plus another for backquote which is sandwiched between
underscore and lowercase 'a'.


Turning domain names into keys
------------------------------

DNS names need some preparation for use as lexical keys. The labels
need to be in reverse order, and the length octets need to be
converted to some kind of non-byte value.

Note that the specially prepared version of the key can be ephemeral, to
save memory at the cost of re-doing name preparation for each lookup.
This is probably the right trade-off since in a DNS server the lookup
key will typically be unknown and fresh off the wire, and less often
re-doing a lookup for a key held in memory.

There are three plausible ways of preparing a DNS name for a qp-trie
lookup key:


### dope vectors

Instead of preparing a name by creating a new string, which can
require doubling the memory used to store names, it might make more
sense to use a "dope vector" that describes the location of the label
boundaries. (I have borrowed the term from multidimensional array
implementations, where the dope vector contains the bounds and strides
of each dimension of an array.)

A domain name dope vector is just a list of the indexes of the length
bytes in reverse order. For example, (using \digit to represent the
length bytes)

        \4grey\3csi\3cam\2ac\2uk\0

needs a dope vector like

        19 16 13 9 5 0

The dope vector for an uncompressed name can use one-byte indexes,
because names are up to 255 bytes long. A (possibly compressed) name
in a packet needs two-byte indexes, because packets can be up to 65535
bytes long, and name compression allows names to be widely scattered.

A domain name can have at most 127 labels not counting the root (1
byte length plus 1 byte contents for each label, plus one byte root
terminator). Each label can be up to 63 bytes long.

The offset of a byte in a name can be represented as a pair of a label
offset and a byte offset within the label, which needs 7 + 6 = 13 bits
with a fixed radix. This fits inside the spare space in the upper branch
node.

Example code to get an byte from a name, given label and byte offsets,
the same descriptor as before, and a descriptor length:

		if(label >= desclen)
			return(-1);
		if(byte >= name[desc[label+1]])
			return(-1);
		return(name[desc[label+1]+byte+1]);

The dope vector is one element longer than the number of labels since
it includes the root terminator. In a bare name this can double as the
name's length.


### stringifying

We can store domain names in a data structure indexed by strings by
re-arranging the name into a string that matches the lexicographic
order of the name, and which is compatible with C. To stringify a wire
format domain name:

  * reverse the order of the labels (but the octets within each label
    remain in the original order)

  * convert all ASCII upper-case letters to lower-case
    (per RFC 4034 sections 6.1 and 6.2)

  * where a \000, \001, or \002 byte appears within a label, escape it
    by prefixing with a \002 byte.

  * separate labels with a \001 byte (instead of the wire format
    length byte or the `.` in presentation format)

  * terminate the name with a \000 byte

So, for example,

        \4grey\3csi\3cam\2ac\2uk\0

becomes

        uk\1ac\1cam\1csi\1grey\0

A stringified domain name can be up to twice as long as a wire format
domain name. (504 bytes to be precise.)


### eager byte-to-bit

To look up a domain name in a qp-trie, we need to scan the name twice:
once to prepare the name, and once while traversing the trie.

During the traversal, we need to convert byte values from the name
into bit values for testing the bitmaps in the index words of each
branch node. For a DNS-trie this is fiddly.

It might make sense to hoist the byte-to-bit conversion out of the
traversal loop into the name preparation loop. This could reduce
register pressure and reduce the risk of the memory system waiting for
the CPU. On the other hand it could be wasted work if the name is
relatively long but the trie is relatively shallow.

The conversion is similar to stringifying:

  * reverse the order of the labels (but the octets within each label
    remain in the original order)

  * convert hostname bytes to a single bit index value between 0 and 63

  * convert non-hostname bytes to two bit index values between 0 and 63,
    for the upper 3 bits and the lower 5 bits

  * convert label separators to the index of the end-of-label bit

I previously wrote a simplified version of the qp-trie traversal loop:

        while(t->isbranch) {
            __builtin_prefetch(t->twigs);
            b = 1 << key[t->offset]; // <-- marked line
            if((t->bitmap & b) == 0) return(NULL);
            t = t->twigs + popcount(t->bitmap & b-1);
        }

Most of the simplification was on the marked line, where I omitted the
logic that extracts the 4-bit or 5-bit nibble from the key. In a
DNS-trie, this involves checking whether it is an upper or lower branch
node, and mapping the byte from the key to a bit value - which for an
upper node might involve testing whether it is a laetter or a digit, or
doing a table lookup.

If the key is eagerly converted to bit indexes then the simplified
traversal loop can be used pretty much verbatim. As a consequence the
distinction between upper and lower branch nodes disappears, and becomes
a matter of how bytes are mapped to bit indexes during name preparation.


### qualitative comparison

It isn't really possible to say which of these works best in practise
without implementing and benchmarking them, but here are some of the
trade-offs that we can see without code.

  * name preparation

    Dope vectors are the simplest.

    Stringifying and eager byte-to-bit are probably the same effort,
    because case-squashing and escaping are about as expensive as
    byte-to-bit conversion.

    There's a trade-off in stringifying between squashing case up front,
    or doing it as part of the byte-to-bit conversion in the traversal loop.

  * traversal loop

    Eager byte-to-bit is the simplest.

    For dope vectors and stringified lookups, it would be interesting to
    find out whether byte-to-bit conversion is faster in code (if 65 <=
    byte && byte <= 90, etc.) or with a lookup table (ctype style).

    Dope vectors require an extra indirection to access the key, but it
    should a fast access to the L1 cache. How much slowdown will it cause?

  * polymorphism

    Stringifying allows one trie implementation to be used for many
    purposes, not just DNS names. (More on that below.)

    Dope vectors imply three trie implementations, for compressed names
    with 2-byte offsets into packets, for 1-byte offsets into
    uncompressed names, and for other strings.


Non-DNS string keys
-------------------

It's possible to use a DNS-trie branch structure for arbitrary string
keys. Upper branch nodes only work for the first few KB of a string,
because of the limited 14 bit size of the offset field. But lower branch
nodes have space for multi-megabyte offsets, 28 bits. So a DNS-trie
could use the hostname layout optimization for shorter keys, and revert
to a 5-bit qp-trie for longer keys.

It's undesirable to do name preparation for long keys, because of the
cost of copying the key (which is likely to bust the L1 cache) and
because the trie traversal loop is not going to need to examine all the
bytes in the key, so much of the effort would be wasted. This implies
that it doesn't make sense to eagerly convert long string keys into bit
indexes.


Key comparisons
---------------

A qp-trie traversal typically finishes by comparing the lookup key
against the key that was found by the traversal.

A DNS server probably wants to store names in wire format, in which case
the qp-trie lookup code will need to pass around both forms of the key:
the prepared key for traversing the trie, and the normal wire-format key
for the final comparison.

With dope-vector name preparation it's necessary to have both the
wire-format name and the dope vector to traverse the trie so this
happens automatically.

With eager byte-to-bit, it isn't hard to keep hold of the wire-format
name for the final comparison.

With stringified keys and a general-purpose trie implementation, it is
weird to have a second form of the key for the final comparison. (More
on this in a moment.)


The cost of polymorphism
------------------------

It might be possible to have multiple different qp trie implementations
that differ just by the code for indexing keys and testing the twig
bitmap, so that there can be a version tuned for DNS names and a
general-purpose version for other strings. The trick for good
performance is to ensure that the key indexing code gets inlined and
optimised nicely. This is bread and butter for C++ and Rust but requires
a bit of stunt coding in C, such as `#include`-based generics.

For the simplest case, stringified keys, it probably makes sense to have
an internal functions that take two versions of the key as arguments, a
traversal string and a comparison string. The general-purpose string
entry points can call the internal versions with the same key twice; and
the DNS-specific entry points can stringify the name, then call the
internal function with stringified and wire-format versions.


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
