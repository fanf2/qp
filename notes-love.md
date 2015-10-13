Some comments from people with nice things to say about qp tries
----------------------------------------------------------------


Marek Vavrusa

<https://twitter.com/vavrusam/status/650967100631175168>

> I use both crit-bit tries and HAT tries, happy to try it against
> them on DNS-like data (though the iteration code looks slow).

<https://twitter.com/vavrusam/status/651015862460260352>

> The @fanf qp-tries are ~20% faster and consume 9% less memory
> (rigged with mempool allocator) than crit-bits in my use case.

<https://twitter.com/vavrusam/status/651017075247443968>

> I think it's going to consume less memory for most folks with stdlib
> allocator, as it's 2x shallower trie and 2x less alloc calls!

<https://twitter.com/vavrusam/status/651359625187622912>

> though both gcc/clang generate popcntl with -msse4.2, beats even HAT
> tries in this test.

<https://twitter.com/vavrusam/status/651414406748852224>

> Really enjoyed toying with @fanf's qp tries today. It's been a while
> working on this sort of stuff...

<https://twitter.com/vavrusam/status/651748801728921600> (prefetching)

> pretty consistent 7% speed bump on my simple benchmark. This is
> shaping up nicely!



Justin Mason

<http://taint.org/2015/10/06/235803a.html>

> Interesting new data structure from Tony Finch.

<https://twitter.com/jmason/status/653294858296295424> (prefetching)

> this is awesome. every time I've tried using tries, the memory
> access patterns vs cache killed its performance

<https://twitter.com/jmason/status/653296399900123136>

> haha, nothing worse than when dumb brute force over an
> integer-indexed array wins ;)



Devon H. O'Dell

<https://twitter.com/dhobsd/status/653933012762005504>

> Been enjoying your critbit / trie articles. Just ran my own
> benchmarks on qp and it is *very* nice. Will be using soon.

<https://twitter.com/dhobsd/status/653934216053288962>

> We've a compelling use case for crit-bit, and I was going to replace
> it with a specialized rbt (which performed better), but qp > *

<https://twitter.com/dhobsd/status/653958255937384449> (embedded crit-bit)

> Oh, also, the cb tree we use is a parentless embedded tree; source
> at https://github.com/glk/critbit (it's a bit of an eyesore though)

<https://twitter.com/dhobsd/status/653958255937384449>

> The "qp trie" by @fanf is amazing and you should use it.
> Preliminary synthetic benchmarks against some alternatives:
> https://9vx.org/post/qp-tries/
