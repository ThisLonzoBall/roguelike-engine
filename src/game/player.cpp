#include "game/player.h"

#include "engine/input.h"
#include "engine/renderer.h"

namespace {

// Tuning. Durations are in simulation ticks (60 per second) so they're exact.
constexpr float kMoveSpeed = 300.0f;   // px/s
constexpr float kDashSpeed = 1400.0f;  // px/s -> 9 ticks covers ~210 px
constexpr int kDashTicks = 9;          // 0.15 s
constexpr int kDashCooldownTicks = 18; // 0.30 s, measured from dash start
constexpr int kDashBufferTicks = 6;    // 0.10 s early-press window

constexpr float kSize = 32.0f;

void startDash(Player& player) {
    player.state = PlayerState::Dashing;
    player.stateTicks = kDashTicks;
    player.dashDir = player.facing;
    player.dashCooldownTicks = kDashCooldownTicks;
    player.dashBufferTicks = 0;
    player.invulnerable = true;  // i-frames; nothing can hit us yet, but enemies will check this
}

}  // namespace

void updatePlayer(Player& player, const Input& input, float dt) {
    player.prevPos = player.pos;

    if (player.dashCooldownTicks > 0) --player.dashCooldownTicks;
    if (player.dashBufferTicks > 0) --player.dashBufferTicks;
    if (input.pressed(Action::Dash)) player.dashBufferTicks = kDashBufferTicks;

    Vec2 move = input.moveAxis();
    if (lengthSq(move) > 0.0f) player.facing = move;

    switch (player.state) {
    case PlayerState::Normal:
        if (player.dashBufferTicks > 0 && player.dashCooldownTicks == 0) {
            startDash(player);
        } else {
            player.pos += move * (kMoveSpeed * dt);
        }
        break;

    case PlayerState::Dashing:
        player.pos += player.dashDir * (kDashSpeed * dt);
        if (--player.stateTicks <= 0) {
            player.state = PlayerState::Normal;
            player.invulnerable = false;
        }
        break;
    }
}

void drawPlayer(Renderer& renderer, const Player& player, float alpha) {
    Vec2 p = lerp(player.prevPos, player.pos, alpha);

    // Body: orange normally, pale blue while dashing so the state is visible.
    Color bodyColor = player.state == PlayerState::Dashing ? Color{170, 220, 255, 255}
                                                           : Color{230, 110, 60, 255};
    renderer.drawRect({p.x - kSize / 2, p.y - kSize / 2, kSize, kSize}, bodyColor);

    // Facing indicator: small square just outside the body.
    constexpr float kDot = 8.0f;
    Vec2 d = p + player.facing * (kSize * 0.75f);
    renderer.drawRect({d.x - kDot / 2, d.y - kDot / 2, kDot, kDot}, {255, 240, 200, 255});
}
