# halva

Front-compressed lexicon.

## Purpose

This is a set-like data structure that uses
[front-coding](https://en.wikipedia.org/wiki/Incremental_encoding) to compress a
lexicon. Such a data structure supports the same operations as an ordered set
(checking for the presence of a word, iterating over the lexicon in
lexicographical order), but is much smaller. The following table shows the size
of a few dictionaries before and after compression:

    dictionary      language    decompressed  compressed
    ---             ---         ---           ---
    Unix            English     920K          384K
    Corriere        Italian     404K          216K
    Duden           German      2.5M          1.4M
    Robert          French      2.0M          748K
    Monier-Williams Sanskrit    2.5M          1.2M

This data structure supports ordered minimal perfect hashing: there is a
one-to-one correspondence between a word and an ordinal representing its
position in the lexicon. This allows finding a word given its ordinal, and,
conversely, finding the ordinal corresponding to a word, as in a sorted array.

Finally, strings containing embedded zeroes are supported.

## Building

There is no build process. Compile `halva.c` together with your source code, and
use the interface described in `halva.h`. You'll need a C99 compiler, which
means GCC or CLang on Unix.

A command-line tool `halva` is included. Compile and install it with the usual
invocation:

    $ make && sudo make install

A Lua binding is also available. See the file `README.md` in the `lua` directory
for instructions about how to build and use it.

## Usage

The C API is documented in `halva.h`. See the file `example.c` for a concrete
example.

## Details

### References

This implementation draws from the chapters on inverted index compression in
each of these books:

* [Manning et al. (2008), Introduction to Information Retrieval](http://nlp.stanford.edu/IR-book/pdf/05comp.pdf).
* [Büttcher et al. (2010), Information retrieval](http://www.ir.uwaterloo.ca/book/06-index-compression.pdf).

### Encoding

Lexica contain three sections: a header, an array of bucket pointers, and a
series of buckets of variable length.

The header contains the following fields, encoded as 32-bit integers, in network
order:

    byte offset   field
    ---           ---
    0             magic identifier (the string "hlva")
    4             data format version (currently, 1)
    8             number of words in the lexicon
    12            size in bytes of the buckets region

The bucket pointers array encodes the position, in the buckets region, of each
nth word in the lexicon, `n` being hardcoded to the value `HV_BLOCKING_FACTOR`.
Pointers are encoded as 32-bit integers, in network order.

The bucket region consists in a series of buckets. Each bucket (except, maybe,
the last one) encodes `HV_BLOCKING_FACTOR` words. The first word of each bucket
is prefixed with a single byte encoding its length. Remaining words are not
written in full. The prefix a given word shares with the word that precedes it
is replaced with one or two byte encoding the length of this prefix and the
number of remaining bytes in the word. When possible, each of these numbers is
stored into a nibble, otherwise a byte.
