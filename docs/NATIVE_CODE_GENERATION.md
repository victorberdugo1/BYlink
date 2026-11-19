# Native Code Generation

Status: **Implemented** with LLVM through Clang as the backend. No interpreter, VM or JIT is involved and none is required.

```
BYlink IR -> LLVM IR text (explicit triple, optional cpu/features) -> clang --target=T -x ir -O<n> [-c] -> object / executable
```

`bylc` needs no LLVM libraries, only an executable Clang that includes the target's backend.

## Stages that are kept separate

1. `--emit-ir`: BYlink IR text, target independent.
2. `--emit-byb`: BYlink IR, binary, target independent.
3. `--emit-llvm`: LLVM IR for the selected target.
4. `--emit-obj`: object file for the selected target.
5. default: linked executable.

## Optimization

`-O0 -O1 -O2 -O3 -Os` choose both the BYlink IR optimizer (skipped at `-O0`) and the Clang level. Default is `-O2`. All five levels are exercised by the native tests and by the AArch64 emulation test. `-O3` is not assumed to be fastest; no measurements exist yet (see `PERFORMANCE_STRATEGY.md`).

## Target specifics

- Triple: always passed with `--target=`; the `target triple` line in the `.ll` is the same value. Previously the line was ignored by Clang.
- CPU and features: function attributes in the `.ll`, so a given `.ll` file reproduces the build. Unknown names are rejected.
- Data layout: not emitted; Clang supplies it for the triple. Safe while the IR has no pointers.
- Traps: `llvm.trap`.

## Not done

Link-time optimization, profile-guided optimization, vectorization control, SIMD operations in the language, instruction-scheduling options, a custom backend. A proprietary backend has not been justified by any measurement and is not planned.
