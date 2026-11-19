# BYGL 2 — compact interactive OpenGL language

BYGL 2 adds a small game loop to the static BYGL scene language:

- `var name initial` declares persistent numeric state.
- `update ... draw ... run` updates state and renders every frame.
- `set` and `add` modify variables.
- `if ... else ... end` provides conditional logic.
- Prefix expressions provide arithmetic, comparisons and boolean operations.
- `key W`, `key UP`, etc. read keyboard state; `dt` is frame delta seconds and `time` is elapsed seconds.
- `box` draws a non-uniform 3D cuboid; `label` draws a numeric state value.
- `circle x y radius r g b [a]` accepts RGB or RGBA; alpha defaults to 1.

`examples/pong.bygl` is a two-player 3D Pong: W/S controls the left paddle, Up/Down controls the right paddle, the ball bounces from walls and paddles, scores are updated, R resets the match and Escape quits.

The single LLM contract for static scenes and interactive programs is `docs/LLM_BYGL_PROMPT.txt`.

## Windows MinGW-w64

Run PowerShell in the project folder:

```powershell
mingw32-make
.\build\bylc.exe examples\pong.bygl -o build\pong.exe
.\build\pong.exe
```

Or use `mingw32-make pong-demo` to build and launch the example.

## Linux

Install GLFW/OpenGL development packages, then run `make pong-demo` from a graphical session.

BYGL 2 is deliberately small, not a general-purpose language. It provides numeric state, a per-frame update block, conditionals, keyboard input, arithmetic expressions and drawing. It does not yet provide arbitrary user functions, arrays, audio, networking, mouse input or custom shaders.
