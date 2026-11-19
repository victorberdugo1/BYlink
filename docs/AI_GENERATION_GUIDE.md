# BYAI generation guide (give this to the model)

Write a BYlink program in BYAI version 1. Output only the program, no prose, no code fences.

## Format

- First line `byai 1`, second line `module NAME`.
- One statement per line. No indentation needed, no comments, no blank lines, ASCII only.
- Expressions are prefix with a fixed number of operands: `add a b`, `lt n 2`, `neg x`, `cast i64 x`, `call f a b`.
- Blocks end with `end`. `if` may have one `else`. For `else if`, write `else` then a nested `if ... end`.

## Types and rules

- Types: `i8 i16 i32 i64 u8 u16 u32 u64 bool void`.
- Every variable has a type and a value: `let x i32 5` (immutable) or `mut x i32 5`. Assign with `set x add x 1`, only to `mut` variables.
- No implicit conversions. Both operands of an operator have the same type; convert with `cast`. A literal takes the type of its context, or add a suffix: `7u8`, `5i64`.
- Conditions are `bool`: write `ne x 0`, never `x`.
- Arithmetic wraps. Division by zero traps. There is no I/O, no shifts, no pointers or arrays: report results with the exit code of `main`.
- Nothing may follow `ret`, `break` or `continue` in the same block. Non-void functions must `ret` on every path (also after a `while`).
- Do not use these words as variable, parameter or constant names: `byai module const fn let mut set if else while scope end ret break continue call cast true false add sub mul div rem and or xor eq ne lt le gt ge land lor neg not bnot` and the type names.

## Operators

`add sub mul div rem and or xor` (integers; `and or xor` also `bool`), `eq ne lt le gt ge` (result `bool`), `land lor` (`bool`, short-circuit), `neg` (signed), `not` (`bool`), `bnot` (integer).

## Template

```
byai 1
module example
const LIMIT i32 10
fn square n:i32 > i32
ret mul n n
end
fn main > i32
mut total i32 0
mut i i32 1
while le i LIMIT
set total add total call square i
set i add i 1
end
ret rem total 256
end
```

## Errors and fixes

Errors look like `file:LINE:COL: error[category]: message` and refer to the BYAI text.

| Message contains | Fix |
|---|---|
| `expected an operand but found end of line` | an operator has too few operands |
| `unexpected 'X'; check operand counts` | an operator has too many operands, or two statements share a line |
| `expected 'end'` | a block is not closed |
| `is reserved` | rename the variable |
| `undefined function` | `call` target is not declared with `fn` |
| `expected 'i32' but found 'i64'` | add `cast` or change the declared type |
| `cannot infer the type of integer literal` | add a suffix such as `5i32` |
| `does not return a value on every path` | add a final `ret` |

Compile with `bylc file.byai -o out`.

## Targets

Do not write target-specific code. The same program is compiled for different CPUs and operating systems by the compiler command line (`--target`), not by changing the source. Integer widths are identical on every target, so a program computes the same results everywhere; only trap behaviour and exit-status width differ (`PORTABILITY_ARCHITECTURE.md`).
