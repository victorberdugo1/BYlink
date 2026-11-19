# BYlink Roadmap

Done (Milestone 1): lexer, parser, semantic analysis, IR, IR optimizer, LLVM emission, native host build, CLI, tests.

Done (portability, phases 0 to 4): audit, `.byb` specification and hardening, target registry and configuration, cross-host `.byb` verification, object generation for 9 targets, Linux AArch64 executables verified under emulation. See `PORTABILITY_AUDIT.md`.

Next, in order:

1. Multiple diagnostics per run and machine-readable diagnostics.
2. Shifts, `u8`-`u64` intrinsics, console output through a system interface.
3. Pointers, arrays and structs with a defined memory model.
4. Store/load promotion in the IR optimizer; inlining.
5. Benchmark suite against C, C++ and Rust with recorded methodology.
6. Remaining portability work: Windows and macOS executables (needs mingw-w64 or an MSVC SDK, and an Apple SDK), ARM32 and RISC-V executables and emulated runs, build and test on Windows and macOS hosts, AArch64 on real hardware, a portable system library (`OS_ABSTRACTION.md`), a pointer and memory model, `.byb` version 1 with sections and a signature if justified.
7. LLM generation study: validity rate, repair turns and tokens for text versus JSON output; JSON schema or constrained grammar.
8. Ed25519-signed license files using an established library; optional managed build.
9. Self-hosting: Stage 0 C++ compiler, Stage 1 BYlink compiler subset, Stage 2 reproducibility comparison.
