Concurrent updates to [qp tries](https://dotat.at/prog/qp)
================================

[Knot DNS uses qp tries](https://gitlab.labs.nic.cz/knot/knot-dns/-/tree/master/src%2Fcontrib%2Fqp-trie)
and a couple of years ago I added
[support for concurrent updates](https://fanf.dreamwidth.org/127488.html).
It supports lock-free multiple reader / single writer updates in RCU
(read/copy/update) style.

It is designed to fit the way authoritative DNS servers work, where
updates to a zone happen one at a time, not too frequently, and
should not affect queries.

The cache in recursive servers is more difficult. When there is a
cache miss, the server needs to resolve the query, and there can be a
lot of resolver jobs in progress that may need to add multiple names
to the trie (e.g. for zones and nameservers they discovered while
resolving the query). And the server needs to clean expired records
from the cache.

It is a much more concurrent setting than an authoritative server.

RCU for qp-trie caches
----------------------

It would be nice if resolver jobs could update the cache without
having to pass all updates to the single writer that is able to update
the qp trie, and without having to wait for a full RCU epoch.

It might be worth using a second data structure that's optimized for
concurrent writes, rather than for storing a lot of data with high
read speed. (A concurrent hash map of some kind, perhaps.)

The idea is that when there is a cache miss on the main qp trie, check
the hash map, and if that lookup also fails, start a resolver job.
Resolver jobs only update the hashmap.

Periodically a cache cleaning job runs, which folds the contents of
the hashmap into the qp trie, and deletes expired records or records
that need to be purged to remain within memory limits. This cleaning
job uses the copy-on-write concurrent qp trie update code.

One question I'm not sure about is how much the leaf objects - the DNS
records structures that are application data from the qp trie point of
view - might need to be updated after they are created, and whether it
matters if they are updated after being moved from the hashmap to the
main trie. For example,

  * When a resolver job is in progress, there needs to be a
    place-holder so that multiple concurrent queries for the same
    records can wait for one resolver to finish.

  * When a cache hit happens on a name that is soon to expire, a
    resolver job should be started to refresh it early. So an existing
    cache entry doubles as a resolver place-holder.

  * When a cache entry has expired and serve-stale is in effect, the
    server will need to keep it around while attempts continue to
    resolve it.

The qp trie is about name lookups, but in the DNS a a name can have
multiple RRsets with different TTLs, so the per-name cache structure
is relatively complicated and needs to support concurrent updates.
Since that is the case, it is probably OK if the per-name record is
moved from the hashmap to the qp trie by the cleaner, even if resolver
jobs are in progress.

---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
