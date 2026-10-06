# Roguelike Engine

A Hades-style action roguelike and the small custom engine underneath it, written in C++20 on top of SDL3 and OpenGL 3.3, with gameplay scripted in Lua.

## Progress

- **Milestone 1** — SDL3 window with a 60 Hz fixed-timestep game loop and interpolated rendering
- **Milestone 2** — Input actions, player state machine, dash with cooldown and input buffering
- **Milestone 3** — Custom OpenGL 3.3 sprite-batching renderer
- **Milestone 4** — World with walls and chasing enemies, circle/box collision, contact hits with i-frames
- **Milestone 5** — Melee attack, health and death, enemy waves, knockback, hitstop, screen shake, particles
- **Milestone 6** — Rooms loaded from text files, locked doors, a run of randomly chosen rooms, hot reload
- **Milestone 7** — Embedded Lua: enemy types and their behaviour are defined in scripts
- **Milestone 8** — Boons (scripted upgrades chosen after each room), player stats as data, bitmap-font text, a UI layer

## Controls

| Action | Keys |
| --- | --- |
| Move | WASD or arrow keys |
| Dash | Space or Left Shift |
| Attack | J or X |
| Choose a boon | A / D or arrow keys, then J or Space |
| Reload rooms and scripts | F5 |
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

## Scripts

Enemy types live in `assets/scripts/*.lua`. Each `Enemy { ... }` block gives a type its stats and, optionally, a `think` function that runs every simulation tick and returns the velocity the enemy wants. The engine keeps doing collision, knockback and damage.

```lua
Enemy {
    name = "chaser",
    hp = 3,
    speed = 140,
    radius = 14,
    color = {120, 200, 90},
    weight = 4,

    think = function(self, ctx)
        if ctx.dist <= ctx.reach then return 0, 0 end
        return ctx.dir_x * self.speed, ctx.dir_y * self.speed
    end,
}
```

The full list of fields, and what `self` and `ctx` contain, is documented at the top of `assets/scripts/enemies.lua`.

Boons live in `assets/scripts/boons.lua`. After each cleared room the game offers three; the one you pick lasts until the run ends. A boon is a name, a description and an `apply` function that edits the player's stats:

```lua
Boon {
    name = "Heavy Blow",
    desc = "Attacks deal +1 damage.",
    max_stacks = 2,
    apply = function(stats)
        stats.attack_damage = stats.attack_damage + 1
    end,
}
```

The stat names and their base values are listed at the top of `boons.lua`.

Scripts run in a sandbox: no file or OS access, a cap on how long one call may run, and `rand()` in place of `math.random` so runs stay repeatable from a seed. A script error is logged with its file and line, and that enemy type falls back to walking at the player until the scripts are reloaded.

## Building

Requires CMake 3.24+ and a C++20 compiler. SDL3 and Lua 5.4 are fetched and built from source on first configure, then linked statically.

```sh
cmake -B build
cmake --build build
```

On Windows you can also open the folder directly in Visual Studio, which picks up `CMakeLists.txt` on its own.

## Layout

```
assets/
  rooms/       room layouts (.room text files)
  scripts/     enemy types, enemy behaviour and boons (.lua)
src/
  main.cpp     entry point and game loop
  engine/      GL loader, renderer and font, input, collision, files, Lua VM, math, RNG
  game/        run and room flow, room parser, script bindings, world, player, UI
```

Engine code never includes game headers; includes are written relative to `src/`.
