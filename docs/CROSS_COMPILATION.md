# Cross Compilation

Host: the machine that runs `bylc`. Target: what the output is for. Object generation needs only Clang with the target's LLVM backend. Linking also needs the target's linker and C runtime.

## Verified: Linux AArch64 from Linux x86-64

Ubuntu 24.04 packages: `clang qemu-user gcc-aarch64-linux-gnu libc6-dev-arm64-cross` (the first two for compiling and running, the rest provide `aarch64-linux-gnu-ld` and the AArch64 glibc).

```
bylc prog.by --target aarch64-linux-gnu --linker aarch64-linux-gnu-ld -O3 -o prog
file prog
qemu-aarch64 -L /usr/aarch64-linux-gnu ./prog
echo $?
```

Object only (needs no linker or runtime):

```
bylc prog.by --target aarch64-linux-gnu --emit-obj -o prog.o
```

Notes from the verification:
- GNU ld works without `--sysroot`. With `--sysroot=/usr/aarch64-linux-gnu`, both GNU ld and lld fail: glibc's `libc.so` is a linker script with absolute paths that are then looked up inside the sysroot. Do not pass that sysroot unless the sysroot contains `/usr/lib` itself.
- `--linker lld` is rejected by Clang's `--ld-path`; name the driver, for example `ld.lld`.
- Running under QEMU user-mode is emulation. Syscall behaviour and timing are not those of hardware.

## Other targets

Object generation works for every registered target (`TARGET_SUPPORT_MATRIX.md`). Executable linking for Windows, macOS, ARM32 and RISC-V was not attempted and is not claimed. An attempt fails with Clang's linker error plus a hint to use `--linker`, `--sysroot` or `--emit-obj`.

## Verification labels

Use these exact words in reports: compiled (object produced), linked, executed under emulation, executed on native hardware.

## Cross-host check of `.byb`

`tests/cross_host_byb.sh` builds `bylc` with `g++`, `aarch64-linux-gnu-g++`, `s390x-linux-gnu-g++` and `riscv64-linux-gnu-g++` (static, run under QEMU) and compares `--emit-byb` output byte for byte. Missing cross compilers are reported as `SKIPPED` and not counted as identical.
