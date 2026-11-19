# Building BYlink

Requirements: C++20 compiler, CMake 3.20+, and for building programs a Clang executable on `PATH` (or `--clang PATH`). Verified: GCC 13.3, Clang 18.1.3, CMake 4.4.5 with Ninja, Ubuntu 24.04 x86-64.

## Linux (verified)

```
sudo apt-get install cmake ninja-build g++ clang
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Result: 70 passed, 0 failed, 0 skipped, with `qemu-user`, `gcc-aarch64-linux-gnu` and `libc6-dev-arm64-cross` installed. Without them the AArch64 execution test is reported as `SKIP` with its reason and is not counted as passed (69 passed, 1 skipped). Without Clang the existing native tests fail, as they did before this work; the new object tests are reported as `SKIP`. Also run clean under `-fsanitize=address,undefined`.

## Windows and macOS (not verified)

`compiler/common/process.cpp` (Win32 `CreateProcessA` and POSIX branches) and `compiler/target/target.cpp` were compile-checked for `x86_64-windows-gnu` and `aarch64-macos` with `zig c++`. Nothing was linked, run or tested on those hosts. The tests invoke programs through `std::system` and `runProcess`, and assume paths and executables that have not been exercised on Windows. Treat Windows and macOS hosts as unverified until the build and test suite are run there.

A `bylc` built for one host does not run on another. Build it on each host you need.

## Cross-host determinism check

```
sudo apt-get install g++-aarch64-linux-gnu g++-s390x-linux-gnu g++-riscv64-linux-gnu qemu-user
sh tests/cross_host_byb.sh
```
