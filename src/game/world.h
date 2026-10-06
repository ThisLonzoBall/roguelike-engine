#pragma once

#include <cstdint>
#include <vector>

#include "engine/math.h"
#include "engine/random.h"
#include "engine/renderer.h"
#include "game/player.h"
#include "game/room.h"

class Input;
class Scripts;

// Stats are copied from the enemy's scripted type when it spawns.
struct Enemy {
    int type = 0;          // index into Scripts::enemyTypes()
    uint32_t id = 0;       // unique within the World; keys the enemy's script state
    int ageTicks = 0;

    Vec2 pos;
    Vec2 prevPos;          // position at the previous tick, for render interpolation
    float radius = 14.0f;  // collision body; also the hurtbox
    Color color;
    bool hasTint = false;  // script override of `color`, e.g. to telegraph an attack
    Color tint;

    int hp = 3;
    int maxHp = 3;
    int contactDamage = 1;

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

// How the current room ended, if it has. The Game layer reacts to this; the
// World itself never loads or switches rooms.
enum class RoomOutcome {
    None,    // still being played
    Exited,  // player walked through an open door
    Died,    // player died and the death animation has finished
};

// The live state of the room being played. Built from a RoomDef and thrown
// away when the room ends. Plain typed lists for now; this is the place a more
// general entity system would grow out of.
struct World {
    std::vector<Rect> walls;        // static level geometry
    std::vector<Rect> doors;        // exits
    std::vector<Rect> solids;       // what bodies collide with: walls, plus doors while locked
    std::vector<Vec2> spawnPoints;
    bool doorsOpen = false;
    int depth = 1;                  // how many rooms into the run this is
    RoomOutcome outcome = RoomOutcome::None;

    Player player;
    std::vector<Enemy> enemies;
    uint32_t nextEnemyId = 1;
    std::vector<Particle> particles;

    Rng rng;                 // all simulation randomness comes from here
    int wavesRemaining = 0;  // waves still to spawn
    int waveDelayTicks = 0;  // countdown to the next wave
    int respawnTicks = 0;    // countdown from death to RoomOutcome::Died

    // Game feel.
    int hitstopTicks = 0;    // simulation is frozen while >0
    float trauma = 0.0f;     // 0..1 screen-shake intensity, decays over time
};

// GPU resources the world is drawn with. Kept apart from World so simulation
// state stays plain data with no dependency on a GL context.
struct WorldTextures {
    Texture floor;
};

// Builds a fresh world from a room template. `depth` scales the number of
// enemies per wave; `seed` drives all randomness inside the room.
World createWorld(const RoomDef& room, int depth, uint32_t seed);
WorldTextures createWorldTextures();

// Advances the whole world by one fixed simulation tick. Enemy types and
// behaviour come from `scripts`.
void updateWorld(World& world, const Input& input, Scripts& scripts, float dt);

// View offset for screen shake. Takes wall-clock time rather than simulation
// time so the shake keeps moving while the simulation is frozen by hitstop.
Vec2 shakeOffset(const World& world, float timeSeconds);

// alpha: 0..1 blend between the previous and current tick.
void drawWorld(Renderer& renderer, const WorldTextures& textures, const World& world,
               float alpha);
