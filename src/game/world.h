#pragma once

#include <cstdint>
#include <vector>

#include "engine/math.h"
#include "engine/random.h"
#include "engine/renderer.h"
#include "game/player.h"

class Input;

// The room is a fixed size in game units; the window scales it to fit.
constexpr float kRoomWidth = 1280.0f;
constexpr float kRoomHeight = 720.0f;

struct Enemy {
    Vec2 pos;
    Vec2 prevPos;          // position at the previous tick, for render interpolation
    float radius = 14.0f;  // collision body; also the hurtbox

    int hp = 3;
    int maxHp = 3;

    Vec2 knockback;        // px/s, decays every tick
    int stunTicks = 0;     // can't chase or deal contact damage while >0
    int hitFlashTicks = 0;
    uint32_t lastHitByAttack = 0;  // Player::attackId of the last swing that hit
};

// Purely cosmetic; never affects gameplay.
struct Particle {
    Vec2 pos;
    Vec2 prevPos;
    Vec2 vel;  // px/s
    float radius = 3.0f;
    int lifeTicks = 0;
    int maxLifeTicks = 1;
    Color color;
};

// Everything that exists in the current room. Plain typed lists for now; this
// is the place a more general entity system would grow out of.
struct World {
    std::vector<Rect> walls;  // static, solid
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Particle> particles;

    Rng rng;                 // all simulation randomness comes from here
    int wave = 0;
    int waveDelayTicks = 0;  // countdown to the next wave once the room is clear
    int respawnTicks = 0;    // countdown to a reset after the player dies

    // Game feel.
    int hitstopTicks = 0;    // simulation is frozen while >0
    float trauma = 0.0f;     // 0..1 screen-shake intensity, decays over time
};

// GPU resources the world is drawn with. Kept apart from World so simulation
// state stays plain data with no dependency on a GL context.
struct WorldTextures {
    Texture floor;
};

// Builds the hard-coded test room. Rooms will come from data files later.
World createWorld();
WorldTextures createWorldTextures();

// Advances the whole world by one fixed simulation tick.
void updateWorld(World& world, const Input& input, float dt);

// View offset for screen shake. Takes wall-clock time rather than simulation
// time so the shake keeps moving while the simulation is frozen by hitstop.
Vec2 shakeOffset(const World& world, float timeSeconds);

// alpha: 0..1 blend between the previous and current tick.
void drawWorld(Renderer& renderer, const WorldTextures& textures, const World& world,
               float alpha);
