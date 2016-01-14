local halva = require("halva")

local lexicon_path = "lexicon.dat"

-- Create a lexicon.
local enc = halva.encoder()
for word in io.lines("/usr/share/dict/words") do
   enc:add(word)
end
enc:dump(lexicon_path)

-- Load the lexicon we just created.
local lexicon = halva.load(lexicon_path)

-- Print all words >= "show".
for word in lexicon:iter("show") do
   print(word)
end

os.remove(lexicon_path)
