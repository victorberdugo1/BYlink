# BYlink Instruction Reference, version 0.1

Source constructs and the IR instruction each one lowers to. IR instructions operate on typed virtual registers (`%N`, assigned once) and typed stack slots (`$N`, one per local variable plus compiler temporaries).

| Source | IR | Semantics |
|---|---|---|
| `a + b` | `add T a, b` | wrapping addition |
| `a - b` | `sub T a, b` | wrapping subtraction |
| `a * b` | `mul T a, b` | wrapping multiplication |
| `a / b` | `div T a, b` | truncating; traps on zero divisor and on signed MIN / -1 |
| `a % b` | `rem T a, b` | truncating remainder; same traps as `div` |
| `a & b`, `a \| b`, `a ^ b` | `and`, `or`, `xor` | bitwise (integers and `bool`) |
| `-a` | `sub T 0, a` | wrapping negation, signed types only |
| `~a` | `xor T a, all-ones` | bitwise not |
| `!a` | `xor bool a, true` | logical not |
| `a == b` etc. | `cmp eq\|ne\|lt\|le\|gt\|ge T a, b` | result `bool`; ordering follows signedness of `T` |
| `a && b`, `a \|\| b` | slot, `condbr`, blocks | short-circuit |
| `a as U` | `cast T a to U` | truncate, sign-extend, zero-extend or reinterpret |
| local read | `load T $s` | |
| `let`, assignment | `store T v, $s` | |
| `f(args)` | `call R @f(T a, ...)` | evaluated left to right |
| `if`, `while`, `break`, `continue` | `br bbN`, `condbr c, bbT, bbF` | |
| `return e` | `ret T v` / `ret void` | |

Valid operand types per instruction are listed in `LANGUAGE_SPECIFICATION.md`, section Types and conversions.
