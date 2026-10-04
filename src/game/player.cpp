#include "game/player.h"

#include "engine/collision.h"
#include "engine/input.h"
#include "engine/renderer.h"

namespace {

// Tuning. Durations are in simulation ticks (60 per second) so they're exact.
constexpr float kMoveSpeed = 300.0f;   // px/s
constexpr float kDashSpeed = 1400.0f;  // px/s -> 9 ticks covers ~210 px
constexpr int kDashTicks = 9;          // 0.15 s
constexpr int kDashCooldownTicks = 18; // 0.30 s, measured from dash start
constexpr int kDashBufferTicks = 6;    // 0.10 s early-press window

constexpr int kHitInvulnTicks = 45;    // 0.75 s
constexpr int kHitFlashTicks = 8;

void startDash(Player& player) {
    player.state = PlayerState::Dashing;
    player.stateTicks = kDashTicks;
    player.dashDir = player.facing;
    player.dashCooldownTicks = kDashCooldownTicks;
    player.dashBufferTicks = 0;
}

}  // namespace

bool isInvulnerable(const Player& player) {
    return player.state == PlayerState::Dashing || player.hitInvulnTicks > 0;
}

void hitPlayer(Player& player) {
    player.hitInvulnTicks = kHitInvulnTicks;
    player.hitFlashTicks = kHitFlashTicks;
}

void updatePlayer(Player& player, const Input& input, std::span<const Rect> walls, float dt) {
    player.prevPos = player.pos;

    if (player.dashCooldownTicks > 0) --player.dashCooldownTicks;
    if (player.dashBufferTicks > 0) --player.dashBufferTicks;
    if (player.hitInvulnTicks > 0) --player.hitInvulnTicks;
    if (player.hitFlashTicks > 0) --player.hitFlashTicks;
    if (input.pressed(Action::Dash)) player.dashBufferTicks = kDashBufferTicks;

    Vec2 move = input.moveAxis();
    if (lengthSq(move) > 0.0f) player.facing = move;

    switch (player.state) {
    case PlayerState::Normal:
        if (player.dashBufferTicks > 0 && player.dashCooldownTicks == 0) {
            startDash(player);
        } else {
            player.pos = moveCircle(player.pos, player.radius, move * (kMoveSpeed * dt), walls);
        }
        break;

    case PlayerState::Dashing:
        player.pos =
            moveCircle(player.pos, player.radius, player.dashDir * (kDashSpeed * dt), walls);
        if (--player.stateTicks <= 0) player.state = PlayerState::Normal;
        break;
    }
}

void drawPlayer(Renderer& renderer, const Player& player, float alpha) {
    Vec2 p = lerp(player.prevPos, player.pos, alpha);

    // Body color shows state: red flash on hit, pale blue while dashing,
    // blinking during the post-hit mercy window, orange otherwise.
    Color bodyColor{230, 110, 60, 255};
    if (player.hitFlashTicks > 0) {
        bodyColor = {255, 70, 70, 255};
    } else if (player.state == PlayerState::Dashing) {
        bodyColor = {170, 220, 255, 255};
    } else if (player.hitInvulnTicks > 0 && (player.hitInvulnTicks / 4) % 2 == 0) {
        bodyColor.a = 110;
    }
    renderer.drawCircle(p, player.radius, bodyColor);

    // Facing indicator: small dot just outside the body.
    renderer.drawCircle(p + player.facing * (player.radius * 1.5f), 4.0f, {255, 240, 200, 255});
}
