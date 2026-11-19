# BYlink AI Instruction Protocol (BYAI), version 1

Status: implemented (`compiler/ai/byai.cpp`), tested (`tests/test_ai.cpp`). BYAI is a second spelling of the BYlink language, not a second language: a BYAI file decodes to exactly the AST the `.by` parser produces and then runs through the same semantic analysis, lowering and optimizer. Semantics are defined by `LANGUAGE_SPECIFICATION.md` and are identical.

## Goals and non-goals

Goals: fewer tokens than source, one canonical spelling, positional errors, rejection of malformed structure instead of guessing. Non-goals: human readability, audio or any non-text channel, a different execution model.

## Lexical form

- ASCII only. Bytes outside 0x20-0x7E (except tab, CR, LF) are a lexical error.
- A file is a sequence of lines; a line is a sequence of words separated by spaces or tabs. Blank lines and indentation are ignored by the decoder. The canonical encoder emits no indentation and no blank lines.
- Word = maximal run of non-whitespace characters. There are no comments and no string literals.

## File structure

```
file   = "byai 1" NL  "module" NAME NL  { const | fn }
const  = "const" NAME TYPE EXPR NL
fn     = "fn" NAME { NAME ":" TYPE } [ ">" TYPE ] NL  { stmt }  "end" NL
stmt   = "let" NAME TYPE EXPR NL
       | "mut" NAME TYPE EXPR NL
       | "set" NAME EXPR NL
       | "if" EXPR NL { stmt } [ "else" NL { stmt } ] "end" NL
       | "while" EXPR NL { stmt } "end" NL
       | "scope" NL { stmt } "end" NL
       | "ret" [ EXPR ] NL | "break" NL | "continue" NL
       | "call" NAME { EXPR } NL
```

`else if` has no spelling; write `else` followed by a nested `if ... end`. `scope` is a nested block (a new variable scope).

## Expressions: fixed-arity prefix, no delimiters

| Form | Arity | Meaning |
|---|---|---|
| `add sub mul div rem and or xor` | 2 | `+ - * / % & \| ^` |
| `eq ne lt le gt ge` | 2 | comparisons, result `bool` |
| `land lor` | 2 | `&&` `\|\|` (short-circuit) |
| `neg not bnot` | 1 | `-x`, `!x`, `~x` |
| `cast TYPE e` | 1 | `e as TYPE` |
| `call NAME e...` | arity of `NAME` | call; the arity comes from the `fn` header |
| `42`, `-5`, `0xFF`, `7u8`, `true`, `false` | 0 | literals (suffix optional, same typing rules as `.by`) |
| `NAME` | 0 | variable or constant |

A statement consumes exactly its line. If an expression ends before the line does, or the line ends before the expression does, the decoder reports an operand-count error. This is the only structural redundancy in BYAI.

## Operand, type, register and memory encoding

- Types: `i8 i16 i32 i64 u8 u16 u32 u64 bool void` (`void` only as a return type, after `>`). Types appear on declarations (`let`, `mut`, `const`, parameters, return) and `cast`; operator types are inferred as in `.by`.
- Registers and memory references do not exist at this level. The BYAI program has named variables only. Typed virtual registers and slots appear in BYlink IR and `.byb`; addressable memory appears in BPIA (`BYLINK_MEMORY_MODEL.md`).
- Reserved words (cannot be variable, parameter or constant names): `byai module const fn let mut set if else while scope end ret break continue call cast true false`, all operator names above, all type names. Function names may use any identifier, including reserved words, because they only occur after `fn` or `call`.
- The canonical encoder renames colliding variable/constant/parameter identifiers by appending `_` until unique. Names are not observable in the IR, so semantics are unchanged; the round-trip guarantee is on IR identity, not on names.

## Dependencies between declarations

Functions and constants may appear in any order. The decoder pre-scans every `fn` header to learn arities before parsing bodies, so forward and recursive calls work. Duplicate names, undefined names and use-before-declaration of constants are reported by the shared semantic checker with the BYAI line and column.

## Module boundaries and version

One module per file, declared on line 2. `byai N` on line 1 selects the protocol version. A decoder rejects a version it does not implement. New operators may be added within a version only if a decoder that does not know them rejects the file (as it does today for unknown words); changing the meaning of an existing word requires a new version.

## Validation

Three layers, all deterministic:

1. Structural (BYAI decoder): header, version, reserved words, operand counts, `end`/`else` matching, known callee, ASCII, nesting depth (limit 256).
2. Semantic (shared with `.by`): types, literal ranges, mutability, reachability, return on all paths, `main` signature.
3. IR verifier (`ir::verify`): SSA-style single definition and dominance, operand types, terminators, branch targets.

## Error reporting

`FILE:LINE:COLUMN: error[CATEGORY]: MESSAGE`, categories `lexical`, `syntax`, `type`, `semantic`, `toolchain`. Line and column are positions in the BYAI text. Only the first error is reported (same as `.by`; multi-error and JSON diagnostics are on the roadmap).

## Tooling

BYAI is target independent: it contains no CPU or OS specific constructs, and the target is chosen only on the `bylc` command line. A program written once compiles for every target in `TARGET_SUPPORT_MATRIX.md` at the level stated there.


```
bylc prog.by --emit-byai -o prog.byai     encode (canonical)
bylc prog.byai -o prog                    compile directly
bylc prog.byai --emit-ir                  inspect IR
bylc prog.byb  -o prog                    compile binary IR
bylc prog.byai --target aarch64-linux-gnu --emit-obj -o prog.o   same program, other target
```
