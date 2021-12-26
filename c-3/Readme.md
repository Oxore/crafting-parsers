# C: (a part of) Lox programming language parser

Implements a part of the Lox programming language grammar. The Lox programming
language was initially developed by Robert Nystrom for his book "Crafting
Interpreters". See
[craftinginterpreters.com](https://craftinginterpreters.com/).

At this point this implementation considered abandoned, because of the approach
taken on it. The approach is basically this: Try minimize as possible usage of a
process' stack to descend into nested expressions while parsing; rather use a
separate parser context with emulated stack that holds all necessary context for
every new level of nesting. Sort of emulated async, I would say.

Initially I believed, that this approach is necessary to make a *reactive*
parser that spits out parsed expression *immediately* as it has been recognized
and then the expression may be passed to an interpreter without making the
interpreter a part of the parser. But then I realized that this statement is not
true.

Thou it is probably not possible to segregate lexer, parser and IO without this
approach. But tying lexer, IO and parser together is probably not such a bad
thing compared to freaking huge single switch-case statement that I've got here.
And now I have to track all the parser's state in my head to not fuck everything
up while adding new language features. Instead I could use just a C language
recursion normally to implement the classic recursive descend parser where
parsing procedure of each statement more or less holds it's context inside
single dedicated C function.

Also I think, it could be better if I started with the top level things like
declarations and statements and made the whole parser in one pass instead of
forgetting how things works after a month of not touching the code. Anyway this
approach would not contribute anything to readability if I was sticking to it
from the beginning.

There is the conclusion of for me: **Do not ever try writing recursive descend
parser for complex non-regular grammars using pseudo-async approach where parser
context holds emulated call stack for nested expressions. It may work on simple
grammars thou, like JSON (c-1).**

The part of the of Lox programming language grammar in Wirth Syntax Notation:

```
statement      = expr-stmt .

expr-stmt      = expression ";" .

expression     = assignment
               | logic_or .
assignment     = [ call "." ] IDENTIFIER "=" expression .
logic_or       = logic_and { "or" logic_and } .
logic_and      = equality { "and" equality } .
equality       = comparison { ( "!=" | "==" ) comparison } .
comparison     = term { ( ">" | ">=" | "<" | "<=" ) term } .
term           = factor { ( "-" | "+" ) factor } .
factor         = unary { ( "/" | "*" ) unary } .
unary          = ( "!" | "-" ) unary | call .
call           = primary { "(" [ arguments ] ")" | "." IDENTIFIER } .
primary        = "true"
               | "false"
               | "nil"
               | "this"
               | NUMBER
               | STRING
               | IDENTIFIER
               | "(" expression ")"
               | "super" "." IDENTIFIER .

arguments      = expression { "," expression } .

NUMBER         = DIGIT { DIGIT } [ "." DIGIT { DIGIT } ] .
STRING         = """" { CHARACTER } """" .
IDENTIFIER     = ALPHA { ALPHA | DIGIT } .
COMMENT        = "//" { CHARACTER } 0x0A .

CHARACTER = " " | "!" | "#" | "$" | "%" | "&" | "'" | "(" | ")" | "*" | "+"
          | "," | "." | "/" | ":" | ";" | "<" | "=" | ">" | "?" | "@" | "["
          | "\" | "]" | "%" | "`" | "{" | "|" | "}" | "~" | "\""" | ALPHA .
DIGIT = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "_" .
ALPHA = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L"
      | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X"
      | "Y" | "Z" | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j"
      | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v"
      | "w" | "x" | "y" | "z" | "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" .
```
