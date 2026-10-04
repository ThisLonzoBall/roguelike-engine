# Roguelike Engine

A Hades-style action roguelike and the small custom engine underneath it, written in C++20 on top of SDL3 and OpenGL 3.3.

## Progress

- **Milestone 1** — SDL3 window with a 60 Hz fixed-timestep game loop and interpolated rendering
- **Milestone 2** — Input actions, player state machine, dash with cooldown and input buffering
- **Milestone 3** — Custom OpenGL 3.3 sprite-batching renderer

## Building

Requires CMake 3.24+ and a C++20 compiler. SDL3 is fetched and built from source on first configure, then linked statically.

```sh
cmake -B build
cmake --build build
```

On Windows you can also open the folder directly in Visual Studio, which picks up `CMakeLists.txt` on its own.

## Layout

```
src/
  main.cpp     entry point and game loop
  engine/      GL loader, renderer, input, math
  game/        player and gameplay code
```

Engine code never includes game headers; includes are written relative to `src/`.
