jumbo branches and qp tries
===========================

There is a possibility of adding support for jumbo branches to qp
tries. Jumbo branches would have more than 16 twigs (sub-tries).

The flag bits can currently take values 0,1,2, so 3 is available to
mark a jumbo branch; as suggested by the flag meanings it would test
a whole byte at a time instead of one nibble at a time. Perhaps the
bitmap field could be used to choose from multiple branch types, in
the style of adaptive radix trees. https://github.com/armon/libart

The key question is how to decide when to coalesce two layers of qp
trie into a jumbo branch. A simple option is to coalesce when the
upper nibble passes some density threshold. This would work OK for
almost-binary keys. However for common ASCII keys, the upper nibble
will usually have four or maybe five possible values. In this case
the upper nibble alone does not provide a clear signal of the density
of the byte, so there is too much risk of wasting time trying to find
one.

Perhaps it would be reasonable to sacrifice lexicographic ordering by
testing lower nibbles before upper nibbles. Then it is quite likely
that a dense byte will fill or nearly fill its first branch. But if it
is OK to sacrifice lexicographic ordering, we might as well use a HAMT
instead.
