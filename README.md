qp tries and crit-bit tries
===========================

I have been working on radix trees / patricia tries / crit-bit tries
with a larger fan-out per branch to reduce lookup costs without
wasting memory.

My best solution so far is the "qp trie", short for quelques-bits
popcount patricia trie. (Nothing to do with cutie cupid dolls or
Japanese mayonnaise!) A qp trie is like a crit-bit trie (aka patricia
trie) except each branch is indexed by a few bits at a time instead of
one bit. The array of sub-tries at a branch node is compressed using
the popcount trick to omit unused branches, saving memory. When
searching a qp trie, the child nodes are prefetched from memory while
the CPU works out which child is next.

The original version of qp tries used 4 bits at a time, so it was a
quadbit popcount patricia trie. There is a faster (but slightly more
complicated) version that uses 5 bits at a time, a quintuple-bit
popcount patricia trie. There is a faster, smaller, and simpler
DNS-optimized version that uses a byte at a time for standard
hostnames.

Based on a few benchmarks, qp tries have about 1/3 less memory
overhead of crit-bit tries, 1.3 words vs 2 words of overhead per item;
the average depth of a qp trie is about half that of a crit-bit trie;
and the overall speed of qp tries is about 30% faster than crit-bit
tries. The qp trie implementation is about 40% bigger.


usage
-----

Type `make test` or `make bench`. (You will need to use GNU make.)
If you have a recent Intel CPU you might want to add `-mpopcnt` to
the CFLAGS to get SSE4.2 POPCNT instructions. Other build options:

* `HAVE_SLOW_POPCOUNT`
	compiles the code to use a hand-coded 16 bit `popcount()`
	instead of `__builtin_popcount()`. No need for this with
	recent clang/llvm; useful with older gcc.

* `HAVE_NARROW_CPU`
	uses a 2 x 16 bit SIMD-within-a-register popcount instead of
	two separate 16 bit popcounts; might be useful on small CPUs
	but makes little difference on 64 bit Intel.

The makefile builds {test,bench}-{qs,qn} with these options; they are
otherwise the same as test-qp and bench-qp.


caveats
-------

Most of the code has only been tested on 64-bit little endian
machines. It might work on 32-bit machines (provided the compiler
supports 64 bit integers) and probably won't work on a big-endian
machine. The "`fn`" (five-bit new) variation should be more portable
since it avoids the mistakes of the earlier code.

Key strings can be byte-aligned but values must be word-aligned; you
can swap this restriction (e.g. if you want to map from strings to
integers) by tweaking the struct layout and adjusting the check in
Tset().

Keys are '\0' terminated C strings, which guarantees one key is not a
prefix of another, so leaves and branches cannot occur at the same
point. It should be possible to support arbitrary binary keys by being
more clever about handling string termination.


articles
--------

Newest at the bottom, scroll down...

* [QP TRIE HOME PAGE](https://dotat.at/prog/qp)

* [2015-10-04](blog-2015-10-04.md) -
	qp tries: smaller and faster than crit-bit tries

	A blog article / announcement.

* [2015-10-07](blog-2015-10-07.md) -
	crit-bit tries without allocation

	An unimplemented sketch of a neat way to use crit-bit tries.

* [2015-10-11](blog-2015-10-11.md) -
	prefetching tries

* [2015-10-13](https://9vx.org/post/qp-tries/) -
	Devon O'Dell benchmarks qp tries against some alternatives

* [2015-10-19](blog-2015-10-19.md) -
	never mind the quadbits, feel the width!

	Benchmarking wider-fanout versions of qp tries.

* [2016-20-23](blog-2016-02-23.md) -
	How does a qp trie compare to a network routing trie?

	Reading some vaguely-related academic literature.

* [2016-03-06](tinytocs.pdf) -
	[TinyToCS](http://tinytocs.org/) vol. 4 includes a paper on QP tries!

	Nicest comment from a reviewer:

	> The body of this paper is a masterpiece of economy:
	> results are presented very clearly and understandably.
	> The result here is simple, compact, and unambiguous,
	> which makes it perfect for TinyToCS.

* 2016-11-21 -
    <https://gitlab.labs.nic.cz/knot/knot-dns/-/merge_requests/574>

	A greatly enhanced and properly engineered implementation of a
	qp trie is being incorporated into CZ.NIC Knot DNS, for better
	memory efficiency.

* 2016-12-20 -
	<https://github.com/jedisct1/rust-qptrie>

	Frank Denis's Rust version of qp tries

* [2017-01-09](blog-2017-01-09.md) -
	qp trie news roundup

* [2020-07-05](blog-2020-07-05.md) -
    A compelling idea: the genesis of my DNS-trie

* 2020-07-20 - <https://github.com/fanf2/nsd>

	A fork of NSD that uses my DNS-trie code. It is significantly
    faster and much smaller than NSD's default radix tree.

	<https://twitter.com/fanf/status/1285181266850205696> -
	a thread about how to optimize a qp-trie for the DNS

* [2021-06-23](blog-2021-06-23.md) -
    Page-based GC for qp-trie RCU

    Memory management to support multithreaded readers

* [2022-05-23 qp guts](2022-05-23-qp-guts.pdf) +
    [2022-05-23 qp api](2022-05-23-qp-api.pdf)

    A couple of qp-trie presentations I gave
    at the 2022 isc.org all-hands meeting;
    the PDFs have the slides with speaker notes

* [2022-06-22](blog-2022-06-22.md) -
    Compacting a qp-trie

    notes on several experiments



thanks
------

Marek Vavrusa (CZ.NIC) and Devon O'Dell (Fastly) enthusiastically put
this code to work and provided encouraging feedback.

Vladimír Čunát incorporated qp tries into CZ.NIC Knot DNS, at the
suggestion of Jan Včelák.

Simon Tatham proved that parent pointers are not needed for embedded
crit-bit tries.


download
--------

You can clone or browse the repository from:

* git://dotat.at/qp.git
* <https://dotat.at/cgi/git/qp.git>
* <https://github.com/fanf2/qp.git>
* <https://git.uis.cam.ac.uk/x/uis/u/fanf2/radish.git>


roadmap
-------

* [Tbl.h][] [Tbl.c][]

	Abstract programming interface for tables with string keys and
	associated `void*` values. Intended to be shareable by multiple
	different implementations.

* [qp.h][] [qp.c][]

	My original qp trie implementation. See qp.h for a longer
	description of where the data structure comes from.

* [fp.h][] [fp.c][]

	5-bit clone-and-hack variant of qp tries.

* [fn.h][] [fn.c][]

	Newer version of 5-bit qp trie, which should be more portable.

	**This is the version that I recommend - faster and less memory overhead**

* [dns.h][] [dns.c][]

	A qp-trie variant optimized for domain names. As well as the
    comments in [dns.h][] there are [some design notes](notes-dns.md).
    The DNS-trie implementation in this repository is heavily bodged
    to fit into my test / benchmark harness so it can be directly
    compared with the other qp-trie versions.

* [wp.h][] [wp.c][]

	6-bit clone-and-hack variant of qp tries.

* [cb.h][] [cb.c][]

	My crit-bit trie implementation. See cb.h for a description of
	how it differs from DJB's crit-bit code.

* [qp-debug.c][] [fp-debug.c][] [fn-debug.c][] [wp-debug.c][] [cb-debug.c][]

	Debug support code.

* [bench.c][] [bench-multi.pl][] [bench-more.pl][] [bench-cross.pl][]

	Generic benchmark for Tbl.h implementations, and benchmark
	drivers for comparing different implementations.

* [test.c][] [test.pl][]

	Generic test harness for the Tbl.h API, and a perl reference
	implementation for verifying correctness.

* [test-gen.pl][] [test-once.sh][]

	Driver scripts for the test harness.


[Tbl.c]:          https://github.com/fanf2/qp/blob/HEAD/Tbl.c
[Tbl.h]:          https://github.com/fanf2/qp/blob/HEAD/Tbl.h
[cb-debug.c]:     https://github.com/fanf2/qp/blob/HEAD/cb-debug.c
[cb.c]:           https://github.com/fanf2/qp/blob/HEAD/cb.c
[cb.h]:           https://github.com/fanf2/qp/blob/HEAD/cb.h
[dns-debug.c]:    https://github.com/fanf2/qp/blob/HEAD/dns-debug.c
[dns.c]:          https://github.com/fanf2/qp/blob/HEAD/dns.c
[dns.h]:          https://github.com/fanf2/qp/blob/HEAD/dns.h
[qp-debug.c]:     https://github.com/fanf2/qp/blob/HEAD/qp-debug.c
[qp.c]:           https://github.com/fanf2/qp/blob/HEAD/qp.c
[qp.h]:           https://github.com/fanf2/qp/blob/HEAD/qp.h
[fp-debug.c]:     https://github.com/fanf2/qp/blob/HEAD/fp-debug.c
[fp.c]:           https://github.com/fanf2/qp/blob/HEAD/fp.c
[fp.h]:           https://github.com/fanf2/qp/blob/HEAD/fp.h
[fn-debug.c]:     https://github.com/fanf2/qp/blob/HEAD/fn-debug.c
[fn.c]:           https://github.com/fanf2/qp/blob/HEAD/fn.c
[fn.h]:           https://github.com/fanf2/qp/blob/HEAD/fn.h
[wp-debug.c]:     https://github.com/fanf2/qp/blob/HEAD/wp-debug.c
[wp.c]:           https://github.com/fanf2/qp/blob/HEAD/wp.c
[wp.h]:           https://github.com/fanf2/qp/blob/HEAD/wp.h
[test-gen.pl]:    https://github.com/fanf2/qp/blob/HEAD/test-gen.pl
[test-once.sh]:   https://github.com/fanf2/qp/blob/HEAD/test-once.sh
[test.c]:         https://github.com/fanf2/qp/blob/HEAD/test.c
[test.pl]:        https://github.com/fanf2/qp/blob/HEAD/test.pl
[bench-cross.pl]: https://github.com/fanf2/qp/blob/HEAD/bench-multi.pl
[bench-more.pl]:  https://github.com/fanf2/qp/blob/HEAD/bench-more.pl
[bench-multi.pl]: https://github.com/fanf2/qp/blob/HEAD/bench-multi.pl
[bench.c]:        https://github.com/fanf2/qp/blob/HEAD/bench.c


notes
-----

* [bitstring keys and longest prefix search](notes-bitstrings-prefixes.md)
* [generic leaf types](notes-generic-leaves.md)
* [rib compression](notes-rib-compression.md)
* [buffered writes and compressed spines](notes-write-buffer.md)
* [jumbo branches](notes-jumbo.md)
* [DNS names](notes-dns.md)
* [concurrent cache updates](notes-concurrency.md)
* [mistakes](notes-mistakes.md)
* [todo](notes-todo.md)

---------------------------------------------------------------------------

Written by Tony Finch <dot@dotat.at> <https://dotat.at/>;
You may do anything with this. It has no warranty.
<https://creativecommons.org/publicdomain/zero/1.0/>
