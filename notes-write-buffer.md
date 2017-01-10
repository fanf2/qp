Buffered writes in [qp tries](http://dotat.at/prog/qp)
=============================

Based on a conversation with [@tef on
Twitter](https://twitter.com/tef), inspired by
<https://www.percona.com/files/presentations/percona-live/PLMCE2012/PLMCE2012-The_Right_Read_Optimization_is_Actually_Write_Optimization.pdf>

@tef wrote,

> so there's this idea of a cache-oblivious b-trie

> and the idea is that you sorta batch writes to get better performance

> that and "linear search of arrays is very fast if you've already
> loaded it into memory"

> so this would be "take a b-trie, add a fixed length insert buffer"
> "write into the buffer first and cascade on overflow" "reads go
> normally but check insert buffer at each depth"

> so you could imagine having a qp-trie where there's a fixed size
> buffer attached to each node

> writes cascade down eventually, which reduces allocation/resizing
> traffic.

> so i was wondering, what if you used the trailing space in the pop
> count array for inserts into the trie

> insert becomes: append to root node, then on full, do real insert
> into child node & so on

> inserting the first 16 elements, it would just fill up the root
> bucket, adding would flip it into a qp-trie node, and allocate
> childs

> you could even do something like having "fixed width nodes" i.e an
> array with a `popcount` at the front, the child nodes as normal, and
> then a write buffer growing from the other end of the array

> the idea being you normally insert into the write buffer until it's
> full, then merge it into the popcount'd array at the front, clearing
> the merge buffer

> well for a start: small tries are kept short, there's much less
> allocation dancing for things in the insert buffer

> the next idea is that you can probably allocate nodes in "power of
> 2" sizes, free list for each

> so when you hit a full buffer, you can pull out something from the
> free list, update it, and return the old one to the free list

> so there might be less pain on the allocator, or smoother progress

> but the real idea is that this would make a trie much more cache friendly

> but i have no idea if that would be true in practice


There are lots of good ideas in here! Before fleshing it out, a
couple of background comments.


Allocation
----------

One of the weaknesses of qp-tries, at least in my proof-of-concept
implementation, is the allocator is called for every insert or delete.
I have not bothered to implement a custom allocator, since internally
malloc() already maintains per-node-size free lists and rounds up
allocation sizes, and I haven't had any good ideas for improving on
that.

There's also a trade-off between small memory size, and realloc()
calls. I have erred on the side of optimizing for small size at the
cost of greater allocator stress.

@tef's suggestion of adding write buffers could be a great way to
change this trade-off.


Search trees vs. radix tries
----------------------------

To search a qp-trie, you only need to look at the trie nodes and the
key you are searching for. You do not need to look at any other keys
until you get to a leaf, where you compare the final key from the trie
with your search key. Stored keys are off to the side of the trie, and
cold keys remain untouched.

However in search trees you need to compare the search key against the
tree keys at each point as you traverse the tree. In B-trees, keys are
kept inline in the tree, and keys near the root of the tree are hot
even if they are never queried.

It would be nice to avoid adding lots of extra full-key comparisons to
a qp-trie search.


Terminology
-----------

Quick reminder of qp-trie structure to make sure we're using
consistent terminology.

A "node" is a two-word structure.

A "leaf" node pairs a key pointer and a value pointer.

A "branch" node contains an index (an offset of a nybble in a key), a
bitmap denoting which nybble values are present in the trie, and a
pointer to an array of nodes, called the branch's "twigs".

        :           :       :
        |           |       |
        |           |       |         +----------+----------+
        | index bmp | twigs |  ---->  |          |          |
        |           |       |         |          |          |
        |           |       |         :          :          :
        :           :       :


Tricky points
-------------

The branch node and its twig array are logically a single structure,
except for the oddity that the metadata about the twig array is put
next to the pointer rather than next to the array. It would perhaps
have been more helpful (specially for comparison with other kinds of
tree) if I had uses the term "node" for this logical structure as a
whole. I'll call it a "logical node" below.

There's an important invariant that the prefixes of all of the keys
are equal up to the index of their common parent branch node.


Write buffers
-------------

Let's add extra space to each twig array to use as a write buffer.

There are two different kinds of insert into a qp-trie:

* realloc() a twig array to make it wider. A write buffer can
  trivially handle this - the write buffer can provide the two extra
  words that we would have asked for with realloc(). Shrink the write
  buffer and grow the twig array within the existing allocation.

* malloc() a new twig array to add a new branch. This is the
  interesting case.

When inserting a new logical node, we need four extra words:

* The key and value pointers of the new leaf

* The new index+bmp word

* The pointer to the new twig array

With a write buffer, we need to use three words:

* The key and value pointers

* An index + metadata word

The latter will be used to speed up searching and later to expand the
write buffer into normal qp nodes.

It is OK that three words doesn't match the uniform two word qp
leaf/branch node size, because the write buffer does not need to be
polymorphic.

The index of the new logical node is between the indexes of its
would-be parent and child. It's best to add it to the child's write
buffer; it would be more awkward to add it to the parent's write
buffer, since then we would have to record and maintain which twig
each write buffer entry relates to.

In a normal branch node, the bitmap relates nybble values to twig
array entries. The metadata needs to be different in a write buffer
entry: we need to know which nybble value belongs to the write buffer
leaf key, and which nybble value matches the child twigs. (A bitmap
would tell us the values but not which is which.) The twig nybble
isn't needed when searching, but it is needed when building the branch
bitmap when the write buffer is expanded to proper branch nodes.


Search algorithm
----------------

When we are following a branch, and before we look at its twig array,
scan the entries in its write buffer. Write buffer entries have
smaller index values than the index of the twig array, so need
checking first. Write buffer entries need to be scanned in index
order.

For each write buffer entry, get a nybble from the search key
corresponding to the write buffer's index.

* If the search nybble is equal to the leaf nybble, stop traversing
  the trie. The final search result depends on whether the leaf key is
  equal to the search key.

* Otherwise, the search key could match another write buffer entry or
  a twig. Keep scanning the write buffer.

After running out of write buffer entries, index the twig array as in
a normal qp trie search.

NOTE: for efficient memory pipelining, the actual order of operations
should be:

* prefetch twig array + write buffer

* calculate child twig index using popcount

* scan write buffer

* use twig index


An insertion gotcha
-------------------

When adding an entry to the write buffer, there is a problem if the
new entry has the same index and leaf nybble as an existing entry.

This would require the search code to do a full comparison against
more than one key, making the innermost search loop more complicated.

After the write buffer is expanded, these two leaves will be off on a
side branch: they aren't on the spine between the parent and child
nodes. A three-word write buffer entry contains enough information to
construct a branch node on the spine when that becomes necessary. But
there isn't enough space to record the information about side
branches. It would be possible to reconstruct side-branch indexes and
bitmaps by re-scanning the keys; this is unnecessary in other
situations.

It's probably preferable to avoid this gotcha by expanding the write
buffer when a new entry collides with an existing one, as if there
were no space for the new entry.


How to expand a write buffer
----------------------------

Some observations:

* If we expand write buffer entries out to classic qp branch + twigs
  layout, we aren't actually avoiding allocations, just delaying them.

* Scanning a write buffer is faster than repeated indirections down a
  spine.

* A three-word write buffer entry is smaller than a four-word branch +
  leaf pair.

This suggests that it's usually better to keep a spine in write buffer
layout as much as possible. When we run out of space in an allocation
(too many twigs or too many write buffer entries), just make it
bigger, without changing the layout.

The layout needs to change when the spine grows a side-branch, as
noted in the previous section.

Another case where it could make sense to change the layout is when
there are many twigs at one point on the spine. e.g.

        +--------------+-------+
        | i=4 bmp wb=3 | twigs |
        +--------------+-------+
                           |
                           V
                  +-----------+-------+
                  |   child   |  ptr  |
                  |   child   |  ptr  |
                  +-----------+-------+-------+
        wr buf    | i=2 nib=1 |  key  | value |
                  | i=2 nib=3 |  key  | value |
                  | i=2 nib=5 |  key  | value |
                  +-----------+-------+-------+

At some point when the fanout gets wide enough a classic qp indexed
indirection will be faster than a linear scan. (benchmarks needed!)
After expansion the layout becomes:

        +--------------+-------+
        | i=2 bmp wb=0 | twigs |
        +--------------+-------+
                           |
                           V
             +--------------+-------+
             |     key      | value |
             | 1=4 bmp wb=0 | twigs | --> +-----------+-------+
             |     key      | value |     |   child   |  ptr  |
             |     key      | value |     |   child   |  ptr  |
             +--------------+-------+     +-----------+-------+


Alternative point of view
-------------------------

After the previous section I think we have strayed from tef's original
suggestion to a different area of the design space.

Instead of a write buffer, we have a compressed spine.

This change of perspective is useful, because it allows us to separate
the idea of spine compression from the idea of using larger
allocations to avoid `realloc()` calls.

A compressed spine reduces the number of indirections needed to scan
past a sequence of leaves.

It can also save space if the spine has at most one leaf at each index
(not counting unused space in the write buffer). It uses the same
space as a classic qp trie layout if there are two leaves at a
particular index, and costs a word more each time the fanout
increases.

So compressed spines can be better than classic qp tries, even when it
is important to minimize space overhead. In this case, use the
compressed spine layout, but do not leave any write buffer space, and
`realloc()` eagerly.


Performance handwaving
----------------------

Good points:

* Reduce allocator calls

* Maybe make the trie shallower -> fewer memory indirections

* Maybe fewer node sizes -> spread across fewer pages by the allocator
  (assuming the allocator is like jemalloc)

Bad points:

* Greater memory size

* More complicated inner search loop

Overall my guess is this will favour update-heavy workloads at the
expense of lookup-heavy workloads, though the change in depth is
likely to lead to surprises.


---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <http://dotat.at/>;
You may do anything with this. It has no warranty.
<http://creativecommons.org/publicdomain/zero/1.0/>
