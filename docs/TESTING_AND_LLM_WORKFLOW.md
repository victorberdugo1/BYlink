# Build, test and ask an LLM for BYlink code

## Requirements

For the BYlink compiler and scalar BYAI programs:
- C++20 compiler (`g++` or `clang++`).
- CMake 3.20+ and GNU Make.
- Clang on `PATH` to compile standard BYlink programs to native executables.

For OpenGL scenes (`.bygl`):
- C++17 compiler (`clang++`, `g++` or `c++`).
- GLFW 3 development files.
- Desktop OpenGL development headers and libraries.
- `pkg-config`.

Debian/Ubuntu/WSL packages:

```sh
sudo apt update
sudo apt install -y build-essential cmake make clang pkg-config libglfw3-dev libgl1-mesa-dev
```

## Build and test

```sh
make
make test
make test-research
make llm-smoke
make bygl-emit-cpp
```

`make test` builds the compiler and runs the C++ test suite, including BYGL parser and C++ generation tests. `make bygl-emit-cpp` parses the OpenGL example and emits generated C++ without requiring GLFW at compile/link time. `make opengl-check` checks whether the GLFW and OpenGL development packages are installed.

## Ask an LLM for an OpenGL scene

Use `docs/LLM_BYGL_PROMPT.txt`. Replace `{{REQUEST}}` with your scene description. Ask the model to return BYGL 2 only, not BYGL 1, C++, GLSL or OpenGL API calls.

Example request:

```text
Create a 3D scene with a large blue rotating cube, two smaller orange and green spheres, a floor grid, a dark background and the title WOLLOWRODK in the upper-left corner.
```

Save the answer as `scene.bygl`. Compile and run:

```sh
./build/bylc scene.bygl -o build/scene
./build/scene
```

Press Escape to close the window. To inspect the generated C++ without linking OpenGL:

```sh
./build/bylc scene.bygl --emit-cpp -o build/scene.cpp
```

If you use another compiler:

```sh
./build/bylc scene.bygl --cxx g++ -o build/scene
```

The OpenGL build uses `pkg-config` to locate GLFW and OpenGL libraries. If those dependencies are absent, `bylc` prints the installation command rather than returning `UNSUPPORTED` for a valid BYGL scene.

## Ask an LLM for a scalar program

Use `docs/LLM_PROMPT_TEMPLATE.txt` for the existing BYAI subset. Replace `{{TASK}}` with one task, save the answer as `program.byai`, then:

```sh
./build/bylc program.byai -o program
./program
printf 'exit code: %s\n' "$?"
```

For `Calculate 1 + 1`, the expected process exit code is `2`.

## Current scope

BYGL v1 supports a real GLFW desktop window, OpenGL 2.1 compatibility rendering, camera, grid, cube, sphere, 3D line, global time-based rotation, 2D rectangle, 2D circle and basic bitmap text. The text font is a small built-in 5x7 ASCII font. It does not yet support arbitrary shaders, textures, loaded 3D models, mouse/keyboard callbacks or general-purpose scene scripting. See `docs/OPENGL.md` for the exact grammar and limitations.

The scalar BYAI path remains separate. Its compiler currently has no general stdin/stdout, pointers, arrays, structs or floating-point support. The Neural Bridge research code is not yet a trained latent communication system; successful compiler and BYGL tests do not prove the neural research hypothesis.
