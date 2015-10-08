* finish HAMT implementation

* revise API to add Tsetkv() which returns the key pointer and
  previous value pointer from the table

* implement embedded crit-bit tries

* implement wp tries, word-wide popcount patricia tries
    * requires an extra word per branch
    * wastes a word per leaf
    * see `notes-embed-key`

* benchmark against other data structures
    * adaptive radix trie
    * HAT-trie
    * hash tables
