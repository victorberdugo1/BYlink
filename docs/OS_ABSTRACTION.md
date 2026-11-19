# Operating System Abstraction

Status: **Designed, not implemented.** The language currently has no I/O, memory allocation, files, threads, networking, time or process services. A program communicates only through `main`'s return value.

## Principles for the future standard library

- The core language (arithmetic, comparison, control flow, calls) stays OS independent. Nothing generic in the IR may name a system call.
- System services are a standard library compiled to native code, with one backend per OS (Linux, Windows, macOS) selected at build time by the target. There is no runtime dispatch layer, VM or JIT.
- Each service states its cost in its definition: whether it allocates, whether it can block, which system library it needs.
- Services in scope, in order: console output, process exit, memory, files, time, threads and synchronization, networking.
- Errors are explicit return values with a portable error type; OS error codes are mapped, not exposed raw.
- Freestanding and bare-metal are separate targets that need startup code, a memory map and hardware support; an ordinary user-space executable does not run without an OS.

## Prerequisites not yet present

A pointer and memory model, aggregate types, a calling-convention definition for foreign functions, and a way to declare external symbols. See `ROADMAP.md`.
