# Portability Audit (Phase 0)

Audited before any change. Environment of the audit: Ubuntu 24.04 x86-64 container, GCC 13.3, CMake 4.4.5, Ninja 1.13.2, Clang 18.1.3, QEMU user-mode 8.2.2, GNU binutils 2.42. This is not the Windows 11 + WSL machine with Clang 21.1.8 described in the project brief; results below were produced here and have not been reproduced there.

## Baseline

Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`, no warnings.

Tests: without Clang installed, 35 passed and 20 failed (all native and CLI tests need Clang). With Clang 18.1.3: 55 passed, 0 failed. The harness counts checks per registered test group, so this number differs from the 44 reported in the brief.

## Answers

**1. Is the IR independent of CPU architecture? Yes, in representation and semantics.** Types are `i8 i16 i32 i64 u8 u16 u32 u64 bool void` with fixed widths. There are no pointers, no `usize`, no floating point, no aggregates, no physical registers. Registers are virtual and single-assignment, mutable state lives in numbered slots. Wrapping arithmetic, casts, literal ranges and division rules are implemented once in `types/types.cpp` with explicit 64-bit masking, not with host `int`. The LLVM emitter emits `add/sub/mul` without `nsw/nuw` (wrapping), and guards `div/rem` explicitly so division by zero and `MIN / -1` trap instead of being LLVM undefined behaviour.

Target-dependent observable behaviour that the IR does not define: how a trap manifests (`llvm.trap` is `ud2` on x86-64, `brk` on AArch64; the process dies with a signal or fault), and how `main`'s value becomes the process exit status (POSIX keeps 8 bits, Windows keeps 32). Both are now listed as target-dependent in `PORTABILITY_ARCHITECTURE.md`.

**2. Is BYB portable between host architectures? Yes by construction, now verified.** The encoder writes bytes one at a time: unsigned LEB128 varints, zig-zag for signed constants, length-prefixed strings. It never dumps structs, never serializes pointers, and has no dependence on `sizeof`, padding or endianness. Verified: `tests/cross_host_byb.sh` builds `bylc` for x86-64, AArch64, riscv64 and big-endian s390x, runs the last three under QEMU user-mode, and compares SHA-256 of `--emit-byb` output for 14 programs at `-O0` and `-O2`. All 28 encodings are identical on all four hosts. Limits of this evidence: three hosts are emulated, and one compiler (GCC 13.3) built all of them.

Defects found in the audit and fixed:
- Wire codes were the C++ enum ordinals (`static_cast<uint8_t>`). Reordering an enum would silently change the format. Replaced with explicit wire tables.
- No upper bound on input size. Added a 64 MiB cap.
- Function and module names from `.byb` were not validated and were pasted into LLVM text (`@"module.name"`). A name containing `"` could inject IR. The verifier now requires ASCII identifiers of at most 1024 bytes.
- Register counts were unbounded. Capped at 2^24.

**3. Host-dependent assumptions in the code.**
- `toolchain.cpp` ran `clang -x ir file.ll` with no `--target`. Clang then **overrides the module's `target triple` with the host triple** (observed: `warning: overriding the module target triple with x86_64-pc-linux-gnu`). Every "target" was silently the host. Fixed: the target is always passed as `--target=`.
- The host triple came from running `clang -dumpmachine`. Replaced by a compile-time host detection in the target registry; no tool is needed to resolve a target.
- Commands were built by string concatenation and run through `popen`/`std::system`, with hand-written quoting that was wrong for Windows (cmd.exe `"` quoting) and a command-injection surface. Replaced by `runProcess(argv)`: `fork/execvp` on POSIX, `CreateProcessA` with correct argument quoting on Windows. No shell is involved.
- `source_filename` was written unescaped into LLVM IR. Now escaped.
- `--emit-byb` and other target-independent outputs evaluated a host-target lookup for the default output name, so they failed on any host not in the registry (found by running the cross-host script on s390x). Fixed.
- Remaining: the pre-existing native tests still run programs through `std::system` with quoted strings. They work on Linux; not changed.

**4. Can the LLVM pipeline target ARM64?** Not before; yes now. Textual IR with an `aarch64` triple was ignored (point 3). With `--target=aarch64-unknown-linux-gnu`, Clang 18 emits a valid AArch64 ELF object (disassembly of `main` for `arithmetic.by` shows `mov w0, #0x36; ret`).

**5. Can Clang here produce Windows or macOS objects?** Yes. Object generation works for x86-64 and AArch64 Windows (COFF) and macOS (Mach-O). Executables cannot be linked here.

**6. What is missing.**
- Windows executables: a mingw-w64 sysroot with a compatible linker, or lld-link plus an MSVC/Windows SDK. Not installed.
- macOS executables: an Apple SDK and Mach-O linker. Not installed, and Apple's SDK licence restricts where it may be used.
- ARM32 and RISC-V executables: a cross C runtime, linker and QEMU for each. Not installed; only objects were produced.
- Real AArch64 hardware: QEMU user-mode is an emulator, not hardware.

**7. What needed modification.** `codegen/toolchain.*`, `codegen/llvm_emitter.*` (signature, attributes, escaping), `driver/pipeline.*`, `driver/main.cpp`, `ir/verify.cpp`, `ai/byb.cpp`, `CMakeLists.txt`, docs. New: `target/target.*`, `common/process.*`.

**8. What stayed unchanged.** Lexer, parser, AST, semantic analysis, lowering, optimizer, BYAI encoder and decoder, the IR data structures, and the IR text printer.

## Implementation plan, as executed

1. Phase 1: write the portable architecture and the target-dependent observables (`PORTABILITY_ARCHITECTURE.md`).
2. Phase 2: harden BYB, specify it (`BYB_FORMAT_SPECIFICATION.md`), test it, verify it across host ISAs.
3. Phase 3: target registry and configuration, `--list-targets`, `--print-target-info`, `--cpu`, `--features`, `--sysroot`, `--linker`.
4. Phase 4: object and executable generation for Linux AArch64; object generation for the other registered targets.

Phases 5 (Windows/macOS linking), 6 (ARM32/RISC-V executables), 7 (system library) and 8 (benchmarks) are not started; see `ROADMAP.md`.
