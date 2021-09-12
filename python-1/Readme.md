# Python: list parser

Grammar in Wirth syntax notation:

```
root = list_contents .
list = open, list_contents , close .
list_contents = element { divider element } [ divider ] .
element = number | list .
open = "[" .
close = "]" .
divider = "," .
number = digit { digit } .
digit = zero | digit_nonzero .
zero = "0" .
digit_nonzero = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" .
```

Spaces, tabs, line feeds and carriage returns are ignored.
