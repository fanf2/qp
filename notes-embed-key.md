embedding keys in qp trie leaves
================================

My implementation of qp tries is intended to be a key-value table.
DJB's crit-bit tries just store a set of strings without associated
values. If you want to use my existing qp trie code to store a set of
strings you will have a wasted word in every the leaf object.

Also, if you implement wp tries (word-wide popcount patricia tries),
again you get a wasted word in every leaf if you use them as a
key-value table, and two wasted words if you use them as a string set.

These wasted words provide a decent amount of space in a leaf object
(16 bytes) to embed short strings in the leaf instead of having an
external pointer. We need some way to mark the difference between an
embedded short leaf and an external long leaf.

On a big-endian system you can put the long-leaf pointer in the upper
word, with a tag set in the least significant bits. The tag means the
last byte of the leaf will be non-zero. In the short-leaf case we can
naturally ensure the last byte is zero by `'\0'` filling unused bytes.
This provides space for short-leaf strings of up to 15 bytes.

On a little-endian system we can't get the tag bits into the last byte
unless we make assumptions about the virtual address space layout. If
the system reserves some part of the address space near the top, we
can put the tag in the top bits - top N bits all 1, where a larger N
means a smaller reserved area. If that isn't possible, we will have to
use the first byte of the leaf for the tag, the last byte for a nul
terminator, leaving 14 bytes for a short-leaf string.

These tag bits probably need to occupy the same space as the
leaf/branch tag. Something like:

* 0 - short leaf
* 1 - upper nibble
* 2 - lower nibble
* 3 - long leaf


API implications
----------------

Embedding strings in leaves means that key strings are copied rather
than borrowed. It is a bad idea to "sometimes" copy them depending on
their length; it would be easier to always copy and take ownership of
the memory used by the key.

This is common in other implementations of table-like APIs, e.g. DJB's
crit-bit tries, and libart adaptive radix tries.
