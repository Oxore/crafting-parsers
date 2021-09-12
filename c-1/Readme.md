# C: pseudo JSON

Implements grammar that parses expressions like this:

```
a:123,b:{c:word1,f1:{somelist:[100,18,1,],},morelist:[{},{q:0}],e:987,},
```

Validates that lists are homogeneous.

Grammar in Wirth syntax notation:

```
root = { map_body }
map = "{" { map_body } "}"
list = "{" { list_body } "}"
list_body = item { "," item } [ "," ]
map_body = map_entry { "," map_entry } [ "," ]
map_entry = word ":" item
item = word | number | map
number = digit { digit } .
word = alphabetic { alphanumeric } .
alphanumeric = alphabetic | digit .
alphabetic =
      "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L"
    | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X"
    | "Y" | "Z" | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j"
    | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v"
    | "w" | "x" | "y" | "z" .
digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" .
```

Spaces, tabs, line feeds and carriage returns are ignored.
