# BYlink Compiler Architecture

```
source (.by, .byai) -> lexer/decoder -> parser -> AST -> semantic analysis -> lowering -> BYlink IR   (.byb enters here)
       -> IR verification -> IR optimization -> [TargetConfig] -> LLVM IR text -> clang --target -> object -> executable
```

| Directory | Responsibility |
|---|---|
| `compiler/common` | diagnostics, version constants, `process` (runs a program from an argument vector, no shell) |
| `compiler/target` | target registry, `TargetConfig`, CPU and feature validation, target descriptions |
| `compiler/ai` | BYAI encoder/decoder and the `.byb` codec |
| `compiler/types` | primitive types and the single implementation of wrapping arithmetic, literal ranges and casts, shared by semantic analysis and the optimizer |
| `compiler/lexer` | tokens with line/column |
| `compiler/ast`, `compiler/parser` | recursive-descent parser, precedence climbing for binary operators |
| `compiler/semantic` | name resolution, type checking, literal typing, constant evaluation, reachability and return checks; annotates the AST with types, local indices and constant values |
| `compiler/ir` | IR data structures, printer, lowering from the annotated AST |
| `compiler/optimizer` | IR passes |
| `compiler/codegen` | LLVM IR text emission for a `TargetConfig`; `toolchain` builds the Clang command for an object or an executable and reports failures |
| `compiler/driver` | `pipeline` (the library entry points used by the CLI and tests) and `main.cpp` (CLI) |

## Backend strategy

`bylc` writes textual LLVM IR and runs `clang --target=<triple> -x ir -O<level> [-c] file.ll -o output` through `runProcess`, without a shell. This needs no LLVM development libraries, only a Clang executable, and Clang performs optimization, register allocation, instruction selection, assembling and linking. Linking against the LLVM C++ API is a possible later change and would be confined to `compiler/codegen`.

## Public and proprietary boundary

Public: grammar, types and semantics, IR text format, AI documentation, CLI. Proprietary (intended): the compiler source and future optimizer, licensing and cloud-build components. No licensing code exists yet; nothing in the language semantics depends on it.

## Testing

`tests/` builds one executable, `bylink_tests`, run by CTest. It covers arithmetic primitives, lexing, parsing, type and semantic errors, IR and optimizer output, LLVM emission, native execution at all optimization levels (exit codes and traps), the examples, and the CLI (exit codes, diagnostics, `--target`, `--emit-*`). `test_portability.cpp` covers `.byb` golden bytes and hostile input, the target registry, object formats for every registered target, toolchain failures, and Linux AArch64 execution under QEMU. Tests that need absent tools are reported as `SKIP`. `tests/cross_host_byb.sh` compares `.byb` output across host CPU architectures.
