# Portability Architecture

Status key: **Implemented**, **Partial**, **Designed** (not implemented), **Not supported**.

```
 .by / .byai / .byb
        |
 lexer, parser, sema, lowering          target independent   Implemented
        |
 BYlink IR + verifier + optimizer       target independent   Implemented
        |                                   (.byb is this IR serialized)
        |  TargetConfig (immutable per compilation)
        v
 LLVM IR text (triple, cpu, features)   target specific      Implemented
        |
 Clang --target=<triple> -c             target specific      Implemented
        |
 object file (ELF / COFF / Mach-O)
        |
 Clang driver + target linker           target specific      Linux AArch64 and host only
        |
 native executable
```

## Host and target

The host runs `bylc`. The target is what the output is for. They are separate: `--target` always selects a registry entry and is passed to Clang explicitly. There is no fallback to the host after an explicit `--target`; unknown names are an error. With no `--target`, the target is the host the `bylc` binary was built for (`host`), decided at compile time from preprocessor macros, so the default is not probed by running a tool.

## Target-independent semantics (Implemented)

- Integers `i8 i16 i32 i64 u8 u16 u32 u64`, `bool`, `void`. Fixed widths on every target.
- Arithmetic wraps modulo 2^width. Division and remainder by zero, and signed `MIN / -1`, trap. Both are refused by the constant evaluator (it returns no value instead of folding) and guarded explicitly in the emitted code, so the behaviour does not depend on LLVM undefined behaviour.
- Casts sign-extend from signed sources, zero-extend from unsigned sources and `bool`, and truncate when narrowing.
- No pointers, floats or aggregates exist, so pointer width, struct layout and floating-point modes do not yet affect semantics. Introducing pointers will require a defined pointer model (**Designed**, see ROADMAP).
- Endianness cannot be observed by any current program. All registry targets are little-endian.

## Target-dependent observables (documented)

| Observable | Dependence |
|---|---|
| How a trap manifests | `llvm.trap`: `ud2` on x86-64, `brk` on AArch64; process killed by signal or exception |
| `main`'s return value as exit status | POSIX keeps the low 8 bits; Windows keeps 32 bits |
| Code size and speed | per backend, CPU and features |

## Target configuration (Implemented)

`TargetConfig` holds the registry entry (name, LLVM triple, architecture, OS, ABI, object format, pointer width, verification level), the CPU, the feature list, the sysroot and the linker. Fields are validated when it is built. The LLVM data layout is not emitted: Clang derives it from the triple, and the current IR contains only integers and stack slots, so no layout-sensitive construct exists. This choice has to be revisited when pointers or aggregates are added.

CPU and features are written into the LLVM IR as `"target-cpu"` and `"target-features"` function attributes, so the compiled result is determined by the emitted `.ll` text alone.

## Binary IR (Implemented)

See `BYB_FORMAT_SPECIFICATION.md` and `BYB_PORTABILITY.md`. `.byb` holds only target-independent IR. No machine code or target container exists, and none is planned in the generic format.

## Not done

Pointer and memory model, atomics, SIMD operations, a portable system library, freestanding mode, and a target-specific container section are **Designed** at most; none is implemented.
