# Current State Audit (baseline, before any change)

Audited: all 47 files in the supplied archive (sources, tests, docs, examples, CMake). Environment: Linux x86-64, GCC 13.3, Clang 18.1.3, CMake 3.28, OpenSSL 3.0.13, Rust 1.97, NASM present.

## Baseline

| Check | Result |
|---|---|
| `cmake -DCMAKE_BUILD_TYPE=Release` + build with `-Wall -Wextra -Wpedantic` | clean, no warnings |
| `ctest` | 1/1 passed, 42 tests passed, 0 failed, about 7 s (native tests compile and run every program at O0, O1, O2, O3, Os) |
| Source size | about 2,500 lines of C++ in `compiler/`, 600 lines of tests |

## Actual pipeline (matches `docs/COMPILER_ARCHITECTURE.md`)

`tokenize` -> `parse` (recursive descent, precedence climbing) -> `analyze` (annotates the AST in place: types, local indices, `constBits`) -> `ir::lower` -> `ir::optimize` (O1+) -> `emitLLVM` (text) -> `clang -x ir -O<n>` -> executable.

Library `bylinkcore` is linked by `bylc` and `bylink_tests`. `pipeline.cpp` holds the library entry points (`compileToIR`, `compileToLLVM`, `buildExecutable`) used by both the CLI and the tests.

## What the IR really is

- Not SSA in the usual sense. Every local, parameter and short-circuit temporary is a typed slot (`$n`); every read is a `load`, every write a `store`. Virtual registers are defined once, but no value flows across blocks except through slots. There are no phi nodes.
- Signedness lives in the type (`i32` vs `u32`), not in the operation.
- No pointer type, no arrays, no addressable memory, no atomics, no vectors, no shifts, no floating point.
- Printable (`--emit-ir`) but there is no parser, no binary form and no verifier. Well-formedness is guaranteed only by construction in `lower.cpp`.
- The optimizer has three passes (constant folding, constant-branch simplification, unreachable-block removal). There is no dead-code elimination and no slot promotion; both are left to LLVM.

## What LLVM emission really does

- Slots become `alloca`; all semantics beyond LLVM's defaults (wrapping arithmetic, trapping division) are made explicit: add/sub/mul carry no `nsw`/`nuw`, division is guarded by a compare and a branch to `llvm.trap`.
- `main` is external, everything else `internal` with name `"module.fn"`.
- No `target datalayout` is emitted; Clang supplies it from the triple.
- Unsigned division emits a redundant `or zero, false`.

## Findings

| # | Severity | Finding |
|---|---|---|
| 1 | High | **Stack overflow on deeply nested input.** `return ((((...1...))));` with 200,000 parentheses crashes `bylc` with SIGSEGV (exit 139). The parser, semantic analysis and lowering are all recursive with no depth limit. Matters for any service that compiles untrusted (LLM-generated) input, including the planned cloud compiler. |
| 2 | Medium | The IR cannot express memory, so the "portable low-level architecture" does not exist yet; nothing below the language can be tested independently of the front end. |
| 3 | Medium | No IR verifier. Any IR producer other than `lower.cpp` (a decoder, a fuzzer, a future frontend) can emit malformed IR that reaches the LLVM emitter and fails inside Clang with an unrelated message. |
| 4 | Medium | Only the first diagnostic is reported. Costly for LLM repair loops (one round trip per error). Already on the roadmap. |
| 5 | Low | Intermediate file is `<output>.tmp.ll` next to the output; two concurrent builds of the same output race. |
| 6 | Low | Toolchain invocation uses `std::system` with shell quoting. Quoting is correct for POSIX, but no timeout, no sandboxing, and stderr of Clang is unfiltered. |
| 7 | Low | `-Os` and `-O3` differ from `-O2` only in Clang flags; BYlink's own optimizer runs identically for all non-O0 levels. |
| 8 | Info | The test harness has a global registry with no name filter or timing; native tests dominate run time. Tests do not use sanitizers or fuzzing. |
| 9 | Info | Only the host triple is accepted; cross-compilation is rejected by design and tested. |

## Test coverage

42 tests: types/arithmetic primitives (5), lexer (4), parser (5), semantic (4), IR and optimizer and LLVM text (9), native execution and CLI (15). Not covered: any IR produced other than by `lower`, parser depth limits, concurrent builds, error paths of the toolchain beyond a foreign `--target`, large inputs.

## Dependencies

Runtime: a `clang` executable. Build: C++20 compiler, CMake. No third-party libraries. Tests are the compatibility contract for everything that follows; none was modified in this work (new tests were added in new files).

## Risks for the planned work

- Any replacement of the LLVM emitter must keep the tested text: `define i32 @main()`, `sdiv i32`, `@llvm.trap()`, `define internal i32 @"t.helper"()`, `target triple`.
- Finding 1 must be fixed before any untrusted-input service (cloud compilation, AI protocol endpoints) is offered.
