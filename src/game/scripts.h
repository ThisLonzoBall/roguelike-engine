#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/math.h"
#include "engine/random.h"
#include "engine/renderer.h"

struct ScriptsImpl;  // the Lua side; defined in scripts.cpp

// An enemy type as defined by a script: stats plus optional behaviour.
//
//   Enemy {
//       name = "chaser",            -- required, unique
//       hp = 3,                     -- hits to kill
//       speed = 140,                -- px/s; passed to think as self.speed
//       radius = 14,                -- body size in px
//       color = {120, 200, 90},     -- r, g, b (0-255)
//       contact_damage = 1,         -- damage dealt by touching the player
//       min_depth = 1,              -- first room of a run this can appear in
//       weight = 4,                 -- relative spawn chance
//       think = function(self, ctx) -- optional; see Scripts::think
//           return ctx.dir_x * self.speed, ctx.dir_y * self.speed
//       end,
//   }
struct EnemyType {
    std::string name;
    int hp = 3;
    float speed = 140.0f;
    float radius = 14.0f;
    Color color{120, 200, 90, 255};
    int contactDamage = 1;
    int minDepth = 1;
    int weight = 1;

    bool hasThink = false;     // false: the engine's built-in chase is used
    bool thinkBroken = false;  // think raised an error; fall back until reload
};

// What the engine tells a think function about its enemy and the world.
struct ThinkInput {
    int type = 0;        // index into Scripts::enemyTypes()
    uint32_t id = 0;     // unique per enemy within a World
    Vec2 pos;
    int hp = 0;
    int ageTicks = 0;
    bool stunned = false;
    float radius = 0.0f;

    Vec2 playerPos;
    float playerRadius = 0.0f;
    float dt = 0.0f;
    int depth = 1;
};

struct ThinkResult {
    Vec2 velocity;          // desired movement, px/s
    bool hasTint = false;   // script set self.tint = {r, g, b}
    Color tint;
};

// The game's scripting layer: loads the .lua files, holds what they define,
// and runs their behaviour functions. Everything Lua-specific stays behind
// this interface.
class Scripts {
public:
    Scripts();
    ~Scripts();
    Scripts(Scripts&&) noexcept;
    Scripts& operator=(Scripts&&) noexcept;

    // Throws away all script state and runs every .lua file in `scriptDir`
    // (sorted by name). Errors are logged with file and line; a file with an
    // error is abandoned at that point and the rest still load.
    // There is always at least one enemy type afterwards: if scripts define
    // none, a built-in chaser is added.
    void load(const std::string& scriptDir);

    const std::vector<EnemyType>& enemyTypes() const;

    // Weighted random choice among the types allowed at `depth`.
    int pickEnemyType(int depth, Rng& rng) const;

    // Runs the enemy type's think function for one enemy on one tick:
    //
    //   think(self, ctx) -> vx, vy
    //
    //   self  a table that lives as long as the enemy does. The engine sets
    //         x, y, hp, age (ticks alive), stunned, speed and radius before
    //         every call; any other field is the script's to keep state in.
    //         Setting self.tint = {r, g, b} overrides the enemy's color
    //         (nil restores it), e.g. to telegraph an attack.
    //   ctx   player_x, player_y, dist (to the player), dir_x, dir_y (unit
    //         vector toward the player), reach (distance at which the bodies
    //         touch), dt, depth.
    //
    // Returns the velocity the enemy wants; nothing (or nil) means stand
    // still. The engine still applies collision, knockback and stun. Scripts
    // get random numbers from the global rand(), which draws from `rng`.
    //
    // If the type has no think function, or it raised an error, the built-in
    // chase is used instead.
    ThinkResult think(const ThinkInput& input, Rng& rng);

    // Per-enemy script state is keyed by enemy id. Call when an enemy dies,
    // and when a new World replaces the old one.
    void forgetEnemy(uint32_t id);
    void forgetAllEnemies();

private:
    std::unique_ptr<ScriptsImpl> impl_;
};
