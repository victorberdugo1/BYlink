# BYlink

AI-oriented programming language and native compiler (`bylc`). Version 0.2.2.

BYlink currently has two generation paths:

- **BYAI v1 (`.byai`)**: compact scalar/program IR compiled through the existing LLVM backend.
- **BYGL v1/v2 (`.bygl`)**: v1 describes static OpenGL scenes; v2 adds per-frame state updates, keyboard input, conditionals and dynamic drawing for interactive games. Native Windows uses Win32/WGL; Linux/macOS use GLFW 3 with OpenGL 2.1 compatibility rendering.

## Build the compiler

Linux/macOS requirements: C++20 compiler and CMake 3.20+. Windows uses MinGW-w64 and GNU Make directly; see the Windows section below.

```sh
make
make test
```

For the native scalar BYAI backend, install Clang. For OpenGL programs, also install GLFW 3, OpenGL development libraries and `pkg-config`.

Debian/Ubuntu/WSL:

```sh
sudo apt update
sudo apt install -y build-essential cmake make clang pkg-config libglfw3-dev libgl1-mesa-dev
```

## Create an OpenGL program with an LLM

Give the LLM `docs/LLM_BYGL_PROMPT.txt` and replace `{{REQUEST}}` with the requested scene. Save its BYGL-only response as `scene.bygl`.

```sh
./build/bylc scene.bygl -o build/scene
./build/scene
```

Press Escape to close the window. An example is included:

```sh
./build/bylc examples/rotating_scene.bygl -o build/rotating_scene
./build/rotating_scene
```

To inspect generated C++ without linking GLFW/OpenGL:

```sh
./build/bylc examples/rotating_scene.bygl --emit-cpp -o build/rotating_scene.cpp
```

See `docs/OPENGL.md` for syntax, supported primitives, dependency setup and limitations.

## Create a scalar program with an LLM

Use `docs/LLM_PROMPT_TEMPLATE.txt` for BYAI v1 programs.

```sh
./build/bylc examples/add.byai -o build/add
./build/add
printf 'exit code: %s\n' "$?"
```

The example exits with code `2`.

## Research status

The repository includes the BYlink compiler, BYAI/BYB formats, benchmark infrastructure and early neural-alignment utilities. It does not yet contain a trained cross-model BNSR encoder/decoder or evidence of teacher-free latent communication. See `research/neural_bridge/` for the current experiments and reports.

## Build natively on Windows

Install MinGW-w64 (C++20) and GNU Make, then open PowerShell in this project folder:

```powershell
mingw32-make
mingw32-make test
.\bylc.exe .\examples\pong_static.bygl -o .\build\pong.exe
.\build\pong.exe
```

The Windows build does not require CMake or GLFW. `mingw32-make` produces both `build\bylc.exe` and a fresh `bylc.exe` in the project root. See `docs/BUILDING_WINDOWS.md`.


## Interactive OpenGL with BYGL 2

BYGL 2 is the single graphics format for both static scenes and interactive programs; static scenes use an empty update block. It adds numeric state, per-frame updates, keyboard input, conditionals and dynamic drawing. See [docs/BYGL2.md](docs/BYGL2.md), [the compact LLM prompt](docs/LLM_BYGL_PROMPT.txt), and [examples/pong.bygl](examples/pong.bygl). Build/run with `make pong-demo` (Linux) or `mingw32-make pong-demo` (Windows MinGW-w64).
