# BYlink Language Specification, version 0.1

## Lexical structure

- Whitespace separates tokens. `//` starts a comment that runs to end of line.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `module fn let mut const if else while return break continue as true false`.
- Type names: `i8 i16 i32 i64 u8 u16 u32 u64 bool void`.
- Integer literals: decimal or hexadecimal (`0x2A`), with an optional type suffix directly attached (`7u8`, `0xFFi64`). A leading `-` directly before a literal is part of the literal.
- Operators and punctuation: `( ) { } , : ; -> = == != < <= > >= + - * / % & | ^ ~ ! && ||`.

## Grammar

```
module      = "module" IDENT ";" { item }
item        = function | constant
constant    = "const" IDENT ":" type "=" expr ";"
function    = "fn" IDENT "(" [ param { "," param } ] ")" [ "->" type ] block
param       = IDENT ":" type
type        = "i8" | "i16" | "i32" | "i64" | "u8" | "u16" | "u32" | "u64" | "bool" | "void"
block       = "{" { stmt } "}"
stmt        = let | assign | if | while | return | "break" ";" | "continue" ";" | call ";" | block
let         = "let" [ "mut" ] IDENT ":" type "=" expr ";"
assign      = IDENT "=" expr ";"
if          = "if" expr block [ "else" ( if | block ) ]
while       = "while" expr block
return      = "return" [ expr ] ";"
expr        = binary
binary      = cast { BINOP cast }
cast        = unary { "as" type }
unary       = ( "-" | "!" | "~" ) unary | primary
primary     = INT | "true" | "false" | IDENT | IDENT "(" [ expr { "," expr } ] ")" | "(" expr ")"
```

`void` is valid only as a function return type. A function without `->` returns `void`.

Binary operator precedence, tightest first (all left associative):

| Level | Operators |
|---|---|
| 8 | `*` `/` `%` |
| 7 | `+` `-` |
| 6 | `&` |
| 5 | `^` |
| 4 | `\|` |
| 3 | `==` `!=` `<` `<=` `>` `>=` |
| 2 | `&&` |
| 1 | `\|\|` |

Unary operators bind tighter than `as`, which binds tighter than every binary operator.

## Types and conversions

- Every expression has exactly one type. There are no implicit conversions.
- Binary operands must have identical types. Result type is the operand type, or `bool` for comparisons and logical operators.
- `+ - * / %` require integers. `& | ^` require integers or `bool`. `== !=` require integers or `bool`. `< <= > >=` require integers. `&& || !` require `bool`. Unary `-` requires a signed integer. `~` requires an integer.
- An unsuffixed integer literal takes its type from context: the declared type of a `let`, `const`, parameter or return type, or the type of the other operand of a binary operator. If no type can be determined, it is an error. A literal must fit its type (`-128` fits `i8`; `128` does not).
- `x as T` converts between integer types and from `bool` to an integer. Integer to `bool` is not allowed; compare with zero. Conversion rule: truncate when narrowing; sign-extend when widening from a signed type; zero-extend when widening from an unsigned type or `bool`; same-width conversion reinterprets the bits.

## Arithmetic semantics

- `+`, `-`, `*` and unary `-` wrap modulo 2^N (two's complement). There is no undefined overflow.
- `/` and `%` truncate toward zero. Division or remainder by zero traps. Signed division or remainder of the minimum value by `-1` traps. A trap terminates the process abnormally (hardware trap instruction).
- `~` flips all bits.
- Comparisons use signed ordering for signed types and unsigned ordering for unsigned types.
- Operands are evaluated left to right. `&&` and `||` short-circuit. `&`, `|`, `^` do not.

## Declarations and scope

- Functions and constants share one global namespace; duplicates are errors. Functions may be used before their definition. A constant may only refer to earlier constants.
- `const` initializers must be compile-time constant: literals, other constants, and unary, binary, comparison and cast operations on them. A constant division by zero is a compile error.
- Variables require an explicit type and an initializer. They are immutable unless declared `let mut`. Parameters are immutable.
- A block opens a scope. Redeclaring a name in the same scope is an error; an inner scope may shadow an outer name. Locals shadow constants.

## Control flow

- `if` and `while` conditions must be `bool`. Braces are mandatory.
- `break` and `continue` are valid only inside `while`.
- Statements after `return`, `break`, `continue`, or an `if`/`else` where both branches terminate, are rejected as unreachable.
- A non-void function must return on every path. A `while` loop is never assumed to terminate the function, even `while true`.

## Entry point and output

A program that builds an executable must define `fn main() -> i32`. Its return value is the process exit code (the operating system may keep only the low 8 bits).

## Diagnostics

Format: `file:line:column: error[category]: message`. Categories: `lexical`, `syntax`, `type`, `semantic`, `toolchain`. The compiler reports the first error only.

## Not yet implemented

Pointers, structs, arrays, shifts, floating point, multiple files, SIMD, atomics, threads, inline assembly, system interface.
