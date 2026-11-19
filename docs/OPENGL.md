# BYlink OpenGL (BYGL) v1 and v2

BYGL is a small scene language compiled by `bylc` into a native C++ program using GLFW 3 and desktop OpenGL 2.1 compatibility rendering. It is separate from BYAI's scalar/function IR: the existing `bylc` executable recognises `.bygl` files and invokes the graphics backend.

## Windows (MinGW-w64)

The Windows backend uses native Win32 + WGL. It does not require GLFW, pkg-config or CMake.
See [BUILDING_WINDOWS.md](BUILDING_WINDOWS.md). From PowerShell in the project folder run `mingw32-make`. It copies the freshly built compiler to both `build\bylc.exe` and the project-root `bylc.exe`. Then compile a scene:

```powershell
.\bylc.exe examples\rotating_scene.bygl -o build\rotating_scene.exe
.\build\rotating_scene.exe
```

## Debian / Ubuntu / WSLg dependencies

```bash
sudo apt update
sudo apt install build-essential clang cmake pkg-config libglfw3-dev libgl1-mesa-dev
```

Build BYlink:

```bash
make
make test
```

Compile a graphics program:

```bash
./build/bylc examples/rotating_scene.bygl -o build/rotating_scene
```

Run it from a graphical Linux desktop or WSLg session:

```bash
./build/rotating_scene
```

Press Escape to close the window. If GLFW cannot initialise, check that the machine has a working OpenGL display/session. `make` and the unit tests do not require a graphical display or GLFW development packages; only compiling a `.bygl` executable requires them.

To inspect the generated C++ without linking OpenGL:

```bash
./build/bylc examples/rotating_scene.bygl --emit-cpp -o build/rotating_scene.cpp
```

To choose the C++ compiler:

```bash
./build/bylc examples/rotating_scene.bygl --cxx g++ -o build/rotating_scene
```

The compiler selection order is `--cxx`, the `CXX` environment variable, then `clang++`, `g++`, and `c++`.

## Syntax

Every program starts with `bygl 1`, declares a window, and ends with `run`. One instruction per line. `#` and `//` start comments outside strings. Strings use double quotes; supported escapes are `\\`, `\"`, `\n`, `\r`, and `\t`.

All color components are floating-point values in the inclusive range 0..1. Coordinates are world units for 3D and pixels for 2D overlays. A default directional-looking light is enabled for solid geometry; grids and 3D lines are drawn unlit.

| Instruction | Arguments | Meaning |
|---|---|---|
| `window` | `width height "title"` | Create a window (64..8192 pixels per dimension) |
| `clear` | `r g b` | Background color |
| `camera` | `eyeX eyeY eyeZ targetX targetY targetZ` | 3D camera position and target |
| `grid` | `divisions spacing r g b` | Grid on the XZ plane at Y=0 |
| `cube` | `x y z size r g b` | Solid cube, `size` is its side length |
| `sphere` | `x y z radius r g b` | Solid sphere |
| `line3` | `x1 y1 z1 x2 y2 z2 r g b` | 3D line |
| `rotate` | `degreesXPerSecond degreesYPerSecond degreesZPerSecond` | Continuous global 3D rotation around the origin |
| `rect` | `x y width height r g b` | Filled 2D overlay rectangle, origin at top-left |
| `circle` | `x y radius r g b` | Filled 2D overlay circle |
| `text` | `x y "text" r g b` | 5x7 bitmap text overlay, rendered at 2x scale |
| `run` | none | Start the render loop; must be last |

Example:

```text
bygl 1
window 960 640 "Rotating scene"
clear 0.02 0.03 0.06
camera 5 4 8 0 0 0
grid 12 1 0.2 0.25 0.35
cube 0 0.5 0 1.5 0.2 0.6 1
sphere 2 0.5 0 0.5 1 0.3 0.1
rotate 0 25 0
text 16 16 "HELLO OPENGL" 1 1 1
run
```

The built-in font covers A-Z, digits, space and common punctuation. Lowercase ASCII letters are rendered as uppercase. Unknown glyphs use a boxed fallback.

## Current scope and limitations

- Desktop OpenGL 2.1 compatibility profile. Windows creates its window/context through native Win32 + WGL; Linux and macOS use GLFW. This is real OpenGL, but it uses legacy fixed-function rendering to keep BYGL compact and broadly available; it is not a modern OpenGL 3.3 shader pipeline.
- Supported geometry: grid, cube, sphere and line. Supported overlays: rectangle, circle and bitmap text.
- Animation: global time-based rotation. The window can be closed with Escape.
- No user-defined shaders, textures, image loading, model loading, mouse/keyboard callbacks, custom mesh buffers, lighting materials, audio or arbitrary loops yet.
- 2D overlays are rendered after the 3D scene. 3D primitives are rendered in declaration order before overlays.
- `.bygl` is not encoded in BYB and does not use the scalar BYAI AST/LLVM backend. `bylc` dispatches it to the dedicated OpenGL C++ backend.
- Windows builds natively with MinGW-w64 and system WGL/OpenGL libraries, without CMake or GLFW. Linux uses GLFW and pkg-config. macOS uses the system OpenGL framework, which is deprecated by Apple.


## Multi-word titles and text (BYGL 0.1.2)

`window` treats all tokens after width and height as the title. `text` treats all tokens between x/y and the final three RGB values as text. Standard ASCII quotes are recommended, but typographic smart quotes and unquoted multi-word strings are also accepted. This prevents editors or model interfaces that alter quotation marks from splitting titles such as `PONG 3D`.


## Interactive programs: BYGL 2

For games and interactive scenes use [BYGL 2](BYGL2.md). It adds persistent numeric state, an update block executed each frame, keyboard queries (`key W`, `key UP`), conditional blocks, arithmetic expressions, dynamic primitives and numeric labels. The BYGL 2 Pong example is `examples/pong.bygl`; build and launch it using `make pong-demo` on Linux or `mingw32-make pong-demo` on Windows. The compact generation contract is `docs/LLM_BYGL_PROMPT.txt`.
