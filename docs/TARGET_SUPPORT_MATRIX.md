# Target Support Matrix

Levels, from weakest to strongest: **object** (valid object of the right format and architecture, header inspected), **linked** (executable produced), **emulated** (executable run under an emulator), **native** (run on real hardware of that architecture). A target is listed at the strongest level that was actually performed. Tests that need tools not present on a machine are reported as `SKIP` and are not counted as passed.

Verified with Clang 18.1.3 on Ubuntu 24.04 x86-64.

| Target name | LLVM triple | Object format | Level | Evidence | Missing for the next level |
|---|---|---|---|---|---|
| `x86_64-linux-gnu` | `x86_64-unknown-linux-gnu` | ELF | native | full test suite, all `-O` levels | none |
| `aarch64-linux-gnu` | `aarch64-unknown-linux-gnu` | ELF | emulated | 4 examples at 5 `-O` levels run under QEMU 8.2.2 user-mode with the Debian cross glibc; exit codes 42, 54, 55, 56 | real AArch64 hardware |
| `x86_64-windows-gnu` | `x86_64-w64-windows-gnu` | COFF | object | COFF machine `0x8664` | mingw-w64 sysroot and linker; Windows or Wine to run |
| `aarch64-windows-gnu` | `aarch64-w64-windows-gnu` | COFF | object | COFF machine `0xAA64` | mingw-w64 (ARM64) sysroot and linker; hardware or emulator |
| `x86_64-apple-darwin` | `x86_64-apple-darwin` | Mach-O | object | magic `0xFEEDFACF`, cputype x86-64 | Apple SDK and linker; macOS to run |
| `aarch64-apple-darwin` | `aarch64-apple-darwin` | Mach-O | object | magic `0xFEEDFACF`, cputype arm64 | Apple SDK and linker; Apple silicon to run |
| `armv7-linux-gnueabihf` | `armv7-unknown-linux-gnueabihf` | ELF32 | object | `e_machine` 40, class 32-bit | ARM32 cross glibc, linker, QEMU |
| `riscv64-linux-gnu` | `riscv64-unknown-linux-gnu` | ELF64 | object | `e_machine` 243 | RISC-V cross glibc, linker, QEMU |
| `riscv32-linux-gnu` | `riscv32-unknown-linux-gnu` | ELF32 | object | `e_machine` 243, class 32-bit | as above |

Not supported: x86-32, MSVC ABI targets, big-endian targets, freestanding and bare-metal targets, WebAssembly, any target not in this table (`bylc` rejects it with `unsupported target`).

Linking a non-host target also depends on the machine running `bylc`: it needs that target's linker and C runtime (`--linker`, `--sysroot`). The `linked` level is a property of one toolchain installation, not of the target alone. The AArch64 result used `aarch64-linux-gnu-ld` 2.42 from Ubuntu's `binutils-aarch64-linux-gnu` and `libc6-dev-arm64-cross`.

Frontend and IR lowering are target independent and are identical for all rows (tested: the emitted LLVM differs between targets only in the `target triple` line).
