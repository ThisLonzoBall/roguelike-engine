# Roguelike Engine

A Hades-style action roguelike and the small custom engine underneath it, written in C++20 on top of SDL3 and OpenGL 3.3.

## Progress

- **Milestone 1** — SDL3 window with a 60 Hz fixed-timestep game loop and interpolated rendering
- **Milestone 2** — Input actions, player state machine, dash with cooldown and input buffering
- **Milestone 3** — Custom OpenGL 3.3 sprite-batching renderer
- **Milestone 4** — World with walls and chasing enemies, circle/box collision, contact hits with i-frames
- **Milestone 5** — Melee attack, health and death, enemy waves, knockback, hitstop, screen shake, particles
- **Milestone 6** — Rooms loaded from text files, locked doors, a run of randomly chosen rooms, hot reload

## Controls

| Action | Keys |
| --- | --- |
| Move | WASD or arrow keys |
| Dash | Space or Left Shift |
| Attack | J or X |
| Reload room files | F5 |
| Quit | Esc |

## Rooms

Each room is a text file in `assets/rooms/`. Any `.room` file in that folder joins the pool the game picks from, so adding a room needs no code change. Edit a file while the game is running and press F5 to reload it.

```
; comment
name: Pillars
waves: 2
map:
###############DD###############
#..............................#
#..E.......P................E..#
...
```

The map is always 32 columns by 18 rows.

| Tile | Meaning |
| --- | --- |
| `#` | Wall |
| `.` | Floor |
| `P` | Player start (exactly one) |
| `E` | Enemy spawn point (at least one) |
| `D` | Door (at least one); locked until every wave is cleared |

A file that doesn't parse is skipped, and the reason is logged with its line number.

## Building

Requires CMake 3.24+ and a C++20 compiler. SDL3 is fetched and built from source on first configure, then linked statically.

```sh
cmake -B build
cmake --build build
```

On Windows you can also open the folder directly in Visual Studio, which picks up `CMakeLists.txt` on its own.

## Layout

```
assets/
  rooms/       room layouts (.room text files)
src/
  main.cpp     entry point and game loop
  engine/      GL loader, renderer, input, collision, files, math, RNG
  game/        run and room flow, room parser, world, player, enemies
```

Engine code never includes game headers; includes are written relative to `src/`.
