#pragma once

#include <span>

#include "engine/math.h"

class Input;
class Renderer;

// The player is always in exactly one state; each state owns its own rules.
// Attacks, specials, hit-stun etc. will be added as further states.
enum class PlayerState {
    Normal,
    Dashing,
};

struct Player {
    Vec2 pos;
    Vec2 prevPos;             // position at the previous tick, for render interpolation
    float radius = 16.0f;     // collision body; also the hurtbox
    Vec2 facing{1.0f, 0.0f};  // last non-zero move direction; dash goes this way

    PlayerState state = PlayerState::Normal;
    int stateTicks = 0;       // ticks remaining in the current timed state

    Vec2 dashDir;
    int dashCooldownTicks = 0;
    int dashBufferTicks = 0;  // >0 means a dash press is waiting to be honored

    int hitInvulnTicks = 0;   // mercy window after taking a hit
    int hitFlashTicks = 0;    // visual feedback only
};

// True while dashing (i-frames) or during the post-hit mercy window.
bool isInvulnerable(const Player& player);

// Registers a hit. Callers check isInvulnerable() first.
void hitPlayer(Player& player);

// Advances the player by one fixed simulation tick, colliding with `walls`.
void updatePlayer(Player& player, const Input& input, std::span<const Rect> walls, float dt);

// alpha: 0..1 blend between the previous and current tick.
void drawPlayer(Renderer& renderer, const Player& player, float alpha);
