# halva-lua

Lua binding of `halva`.

## Building

Check the value of the variable `LUA_VERSION` in the makefile in this directory.
Then invoke the usual:

    $ make && sudo make install

You can also pass in the correct version number on the command-line:

    $ make LUA_VERSION=5.3 && sudo make install LUA_VERSION=5.3

## Usage

See the file `example.lua` in this directory for an introductory example. The
following is a more formal description.

### Constants

`halva.VERSION`  
The library version.

`halva.MAX_WORD_LEN`  
Maximum allowed length of a word.

### Lexicon encoder

`halva.encoder()`  
Allocates a new lexicon encoder and returns it.

`encoder:add(word)`  
Adds a new word to the lexicon. Words must be added in lexicographical order.
The length of a word must be > 0 and < `halva.MAX_WORD_LEN`.

`encoder:dump(path)`  
Dumps a lexicon to a file. Returns `true` on success, `nil` plus an error
message otherwise. The encoder is freezed after this function is called, so no
new words should be added afterwards, unless `encoder:clear()` is called first.

`encoder:clear()`  
Clears an encoder. After this is called, the encoder object can be used again to
encode a new set of words.

### Automaton

`halva.load(lexicon_path)`  
Loads a lexicon from a file. On error, returns `nil` plus an error message,
otherwise a lexicon handle.

`lexicon:locate(word)`  
Returns the ordinal corresponding to a word, if this word is present in the
lexicon. Otherwise, returns `nil`.

`lexicon:extract(position)`  
Returns the word at a given position in a lexicon, if the position is valid.
Otherwise, returns `nil`. A negative value can be given for `position`. -1
corresponds to the last word in the lexicon, -2 to the penultimate, and so on.

`lexicon:size()`  
`#lexicon`  
Returns the number of words in a lexicon.

`lexicon:iter([from])`  
Returns an iterator over a lexicon. If `from` is not given or `nil`, the
iterator will be initialized to iterate over the whole lexicon. If `from` is a
string, iteration will start at this string if it is present in the lexicon,
otherwise just after it. If `from` is a number and the automaton is numbered,
iteration will start at the corresponding position in the lexicon. Negative
positions are valid: -1 corresponds to the last word in the lexicon, -2 to the
penultimate, and so on.  
Examples:

    -- Iterate over all words.
    for word in lexicon:iter() do print(word) end
    -- Iterate over all words >= "diction".
    for word in lexicon:iter("diction") do print(word) end
    -- Iterate over all words, starting at the 333th.
    for word in lexicon:iter(333) do print(word) end
