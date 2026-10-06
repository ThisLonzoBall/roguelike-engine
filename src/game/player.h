#pragma once

#include <cstdint>
#include <span>

#include "engine/collision.h"
#include "engine/math.h"
#include "game/stats.h"

class Input;
class Renderer;

// The player is always in exactly one state; each state owns its own rules.
enum class PlayerState {
    Normal,
    Dashing,
    Attacking,  // wind-up -> active (hitbox out) -> recovery
    Dead,
};

struct Player {
    Vec2 pos;
    Vec2 prevPos;             // position at the previous tick, for render interpolation
    float radius = 16.0f;     // collision body; also the hurtbox
    Vec2 facing{1.0f, 0.0f};  // last non-zero move direction; dash and attacks go this way

    PlayerStats stats;        // base values plus boons; set by the Game
    int hp = PlayerStats{}.maxHp;

    PlayerState state = PlayerState::Normal;
    int stateTicks = 0;       // ticks remaining in the current timed state

    Vec2 dashDir;
    int dashCooldownTicks = 0;
    int dashBufferTicks = 0;    // >0 means a dash press is waiting to be honored
    int attackBufferTicks = 0;  // same, for attack

    // Incremented per swing. Enemies remember the last id that hit them so one
    // swing can't damage the same enemy on several ticks.
    uint32_t attackId = 0;

    Vec2 knockback;           // px/s, decays every tick
    int hitInvulnTicks = 0;   // mercy window after taking a hit
    int hitFlashTicks = 0;    // visual feedback only
};

// True while dashing (i-frames), during the post-hit mercy window, or dead.
bool isInvulnerable(const Player& player);

// Applies damage and knockback along `awayDir` (unit vector). Callers check
// isInvulnerable() first. Sets state to Dead when hp runs out.
void hitPlayer(Player& player, Vec2 awayDir, int damage);

// True on the ticks where the current swing's hitbox exists.
bool isAttackActive(const Player& player);
Circle attackHitbox(const Player& player);

// Latches dash/attack presses into their buffers without advancing anything.
// Called on ticks where the simulation is frozen (hitstop) so presses made
// during the freeze aren't lost.
void bufferPlayerInput(Player& player, const Input& input);

// Advances the player by one fixed simulation tick, colliding with `walls`.
void updatePlayer(Player& player, const Input& input, std::span<const Rect> walls, float dt);

// alpha: 0..1 blend between the previous and current tick.
void drawPlayer(Renderer& renderer, const Player& player, float alpha);
