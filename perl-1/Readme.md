# Perl: map notation

Implements grammar that parses expressions like this:

```
a=0x00059fF,b=00007106,cc={mask=0b111101,val=29812,},y={c=0},ee=0b01
```

Grammar in Wirth syntax notation:

```
root = { map_body }
map = "{" map_body "}"
map_body = map_item { "," map_item } [ "," ]
map_item = word "=" number
number = number_bin | number_hex | number_dec
number_bin = "0b" digit_bin { digit_bin } | zero .
number_hex = "0x" digit_hex { digit_hex } | zero .
number_dec = digit_dec { digit_dec } .
word = alphabetic { alphanumeric } .
alphanumeric = alphabetic | digit_dec .
alphabetic =
      "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L"
    | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X"
    | "Y" | "Z" | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j"
    | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v"
    | "w" | "x" | "y" | "z" .
digit_hex = digit_dec
    | "A" | "B" | "C" | "D" | "E" | "F" | "a" | "b" | "c" | "d" | "e" | "f" .
digit_dec = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" .
digit_bin = "0" | "1" .
```

Spaces, tabs, line feeds and carriage returns are ignored.
