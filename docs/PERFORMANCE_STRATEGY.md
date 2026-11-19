# Performance Strategy

Status: **no benchmarks have been run.** No claim is made that BYlink output is faster or slower than C, C++ or Rust.

## Constraints on the design

No mandatory interpreter, VM, JIT, instruction decryption or runtime indirection in the hot path. Signing and license checks, if built, run outside program execution. The existing generated code is straight LLVM output; the only inserted runtime work is the division guard, which the LLVM optimizer can remove when the divisor is known non-zero.

## Plan

1. Reproducible benchmark harness that separates compile time from run time, records CPU model, frequency policy, compiler versions and flags, and reports medians with spread over repeated runs.
2. Equivalent C and C++ programs for the 10 programs in `bench/corpus`, compiled with the same Clang at the same `-O` level, checked for identical results.
3. Per-target runs on real hardware. Emulated runs (QEMU) measure the emulator, not the CPU, and must not be used for speed.
4. Only then: target-specific tuning (`--cpu`, `--features`), LTO, PGO, vectorization and SIMD, and IR optimizer work (store/load promotion, inlining).

`bench/ai_bench.cpp` measures BYAI and BYB compile-side costs, not generated-program speed.
