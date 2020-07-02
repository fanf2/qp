Mistakes were made in my [qp trie](https://dotat.at/prog/qp)
==================================

I addressed many of these mistakes when I refactored the qp-trie
implementation in Knot DNS to support concurrent access, but I have
not done so for this experimental implementation. (The code here is
public domain; Knot DNS is GPL.)


Terminology
-----------

I originally used "index" to refer to the position of a nybble in a
key, but that didn't leave me with a good word to describe the word
that summarizes a node. Using "offset" for the position of a nybble
allows me to use "index" for the word as a whole, which I like better
because it's like a miniature database index, where a node is like a
miniature database table.

  * word

    either a pointer or an index word, typically 64 bits

  * index word

    contains metadata about a twig array, including key offset and bitmap

  * key offset

    identifies the nybble within a key that is checked against the index

  * nybble

    originally a string of 4 bits but now 5 bits from the key

  * twig

    either a leaf or a branch

  * leaf

    a pair of a key and a value

  * branch

	a pair of an index word and a pointer to a node

  * node

    a vector of twigs


Unions and bit fields
---------------------

A disastrous choice for portability.

It is _much_ better to define the index word as a large-enough integer
type, and use macros or inline functions to extract or update fields
within it.

This makes it trivial to ensure that the tag bits appear in the least
significant bits of the word, without endianness issues.

In a leaf the index word is not an index but instead is a pointer to a
key or a value (depending on which guarantees word alignment), and
it's just as easy to cast the integer to a pointer as it is to access
a field of a union.

The large-enough integer type is at least `uint64_t`, though it needs
to be `uintptr_t` if the platform's pointers are bigger than that (for
example, CHERI capabilities).


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
