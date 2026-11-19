# BYGL 2 verification record

Validated in the build environment on 2026-11-19:

- BYlink compiler built with CMake and GCC 14.2.
- Test executable: 80 passed, 0 failed, 1 skipped (the skipped test requires the AArch64 cross-linker and QEMU).
- `examples/rotating_scene.bygl` (BYGL 1) still emits C++.
- `examples/pong.bygl` (BYGL 2) parses and emits a per-frame state update, keyboard checks, paddle/ball collision conditions, scoring and dynamic drawing.
- Generated Pong C++ passed C++17 syntax checking for both the GLFW/Linux and Win32/WGL conditional branches using API declaration stubs.

Not verified in this environment:

- Launching a real OpenGL window; GLFW development headers/libraries and a display server are not installed here.
- A native MinGW build on Windows; MinGW-w64 is not installed in this environment.

Therefore this confirms parser, compiler integration, unit tests and generated C++ syntax. It does not claim a real Windows or Linux game window was launched during verification.
