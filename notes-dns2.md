More thoughts on DNS and [qp tries](https://dotat.at/prog/qp)
===================================

I previously wrote down a couple of ideas for [DNS and qp-tries](notes-dns.html):

  * two kinds of branch nodes, for common and unusual name octets

  * indexing names lexically using descriptors

Here are some more thoughts about tries for DNS and other strings.

On two implementations
----------------------

It's possible to have two different qp trie implementations that
differ just by the code for indexing keys and testing the twig bitmap,
so that there can be a version tuned for DNS names and a
general-purpose version for other strings. The trick for good
performance is to ensure that the key indexing code gets inlined and
optimised nicely. This is bread and butter for C++ and Rust but
requires a bit of stunt coding in C, such as `#include`-based
generics.

Alternatively it's probably simpler (and maybe faster) to rewrite DNS
names into lexically-ordered form. That is, reverse the labels so a
name like `cam.ac.uk` becomes `uk.ac.cam`, and change from wire-format
counted labels to an escaping scheme like,

  * key octet 0 = end of string
  * key octet 1 = end of label
  * key pair 2 0 = label octet 0
  * key pair 2 1 = label octet 1
  * key pair 2 2 = label octet 2

The stored form of keys can still be normal wire format, if the
qp-trie lookup code passes around both versions of the key: the
revered/escaped key for traversing the trie, and the normal
wire-format key for the final comparison. The reversed/escaped key can
be ephemeral, like a label descriptor would be.

On case sensitivity
-------------------

There are two ways to implement DNS-style ASCII case insensitivity:
either map both upper and lower case to the same bits in the qp trie
bitmap (which is probably best if the lookup code is consuming wire
format names directly), or rewrite the name to lower case (which
allows the same lookup code to be used for case-sensitive or
insensitive lookups depending on how the name is prepared).

Keeping a distinction between a lookup key and the original key
(e.g. reversed/escaped vs. wire format) helps to support being
case-insensitive and case-preserving.

On long strings
---------------

The DNS-tuned qp trie uses two kinds of branch node, one with a 47- or
48-wide bitmap for normal DNS characters, and the other with a 32-wide
bitmap for unusual octets. The normal-case bitmap doesn't have space
to support long string keys, up to 2^13 - 2^15 bytes. This is still
enough for most purposes, but it would be nice to lift the limit.

One way to do that is to structure the trie like a 5-bit qp trie for
long strings. In the basic DNS trie, the 32-wide bitmap always uses
the bottom 5 bits of an octet, but in the 5-bit qp trie the
the quinnybbles can have any alignment and can span bytes. (It's
fiddly, but it's faster than 4-bit qp tries because it requires fewer
memory accesses.)

The phase change between shorter and longer strings will not affect
the lookup code, though it will require some care when adding nodes to
the trie.

Using 32-wide nodes allows keys of up to 2^27 bytes, which should be
enough.
