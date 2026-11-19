# BYlink Specification for AI Agents, version 0.1

Generate BYlink source (`.by`) following these rules. The full grammar is in `LANGUAGE_SPECIFICATION.md`.

## Rules

1. First line: `module NAME;`.
2. Define `fn main() -> i32 { ... }`. Its return value is the exit code.
3. Every variable has an explicit type: `let x: i32 = 5;` or `let mut x: i32 = 5;`.
4. Every statement ends with `;`. Every `if`/`while` body uses braces.
5. Conditions are `bool` only. Write `x != 0`, never `x`.
6. No implicit conversions. Convert with `as`: `(a as i64)`.
7. Both operands of a binary operator have the same type.
8. Give integer literals a type through context or a suffix (`5i64`, `255u8`). `1 < 2` fails; `1i32 < 2` works.
9. Do not write anything after `return`, `break` or `continue` in the same block.
10. Non-void functions must end with a `return` on every path, including after a `while`.
11. Arithmetic wraps. Division by zero traps.
12. There is no I/O, no shifts, no pointers or arrays. Use the exit code to report results.

## Valid types

`i8 i16 i32 i64 u8 u16 u32 u64 bool void`

## Valid statements

```
let [mut] NAME: TYPE = EXPR;
NAME = EXPR;
if EXPR { ... } [else if EXPR { ... }] [else { ... }]
while EXPR { ... }
return [EXPR];
break;
continue;
CALL(...);
{ ... }
```

## Valid expressions

Literals (`42`, `0xFF`, `7u8`, `-5`, `true`, `false`), variables, constants, calls `f(a, b)`, parentheses, unary `- ! ~`, binary `* / % + - & ^ | == != < <= > >= && ||`, casts `e as T`.

## Template

```
module example;

const LIMIT: i32 = 10;

fn square(n: i32) -> i32 {
    return n * n;
}

fn main() -> i32 {
    let mut total: i32 = 0;
    let mut i: i32 = 1;
    while i <= LIMIT {
        total = total + square(i);
        i = i + 1;
    }
    return total % 256;
}
```

## Common errors and fixes

| Diagnostic | Fix |
|---|---|
| `error[type]: expected 'i32' but found 'i64'` | add `as i32` or change the declared type |
| `error[type]: cannot infer the type of integer literal` | add a suffix or use the literal where a type is expected |
| `error[type]: operands of '+' have different types` | cast one operand |
| `error[semantic]: cannot assign to immutable variable` | declare with `let mut` |
| `error[semantic]: unreachable statement` | remove code after `return`/`break`/`continue` |
| `error[semantic]: function ... does not return a value on every path` | add a final `return` |
| `error[syntax]: expected ':' (variables need an explicit type)` | write `let x: TYPE = ...;` |

Compile: `bylc file.by -o out`. The compiler is deterministic and reports the first error with line and column.

## Machine-readable form

A JSON schema or constrained-decoding grammar has not been produced yet; the grammar in `LANGUAGE_SPECIFICATION.md` is the contract. No measurements of LLM validity rate or token counts exist yet.
