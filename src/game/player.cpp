#include "game/player.h"

#include "engine/input.h"
#include "engine/renderer.h"

namespace {

// Tuning that boons can't change; the rest lives in PlayerStats. Durations are
// in simulation ticks (60 per second) so they're exact.
constexpr float kDashSpeed = 1400.0f;  // px/s -> 9 ticks covers ~210 px
constexpr int kDashTicks = 9;          // 0.15 s
constexpr int kInputBufferTicks = 6;   // 0.10 s early-press window (dash and attack)

// Attack phases. Only the active phase has a hitbox; recovery (a stat) is the
// commitment cost and can be cancelled into a dash.
constexpr int kAttackStartupTicks = 3;
constexpr int kAttackActiveTicks = 4;
constexpr float kAttackLungeSpeed = 90.0f;  // px/s forward during wind-up and active

constexpr float kKnockbackSpeed = 420.0f;   // px/s initial, when the player is hit
constexpr float kKnockbackDecay = 0.82f;    // per tick
constexpr int kHitFlashTicks = 8;

int attackTotalTicks(const Player& player) {
    return kAttackStartupTicks + kAttackActiveTicks + player.stats.attackRecoveryTicks;
}

void startDash(Player& player) {
    player.state = PlayerState::Dashing;
    player.stateTicks = kDashTicks;
    player.dashDir = player.facing;
    player.dashCooldownTicks = player.stats.dashCooldownTicks;
    player.dashBufferTicks = 0;
}

void startAttack(Player& player) {
    player.state = PlayerState::Attacking;
    player.stateTicks = attackTotalTicks(player);
    player.attackBufferTicks = 0;
    ++player.attackId;
}

bool canDash(const Player& player) {
    return player.dashBufferTicks > 0 && player.dashCooldownTicks == 0;
}

// Ticks elapsed in the current swing, counting the current tick (1-based).
int attackElapsed(const Player& player) { return attackTotalTicks(player) - player.stateTicks; }

}  // namespace

bool isInvulnerable(const Player& player) {
    return player.state == PlayerState::Dashing || player.state == PlayerState::Dead ||
           player.hitInvulnTicks > 0;
}

void hitPlayer(Player& player, Vec2 awayDir, int damage) {
    player.hp -= damage;
    player.hitFlashTicks = kHitFlashTicks;
    if (player.hp <= 0) {
        player.hp = 0;
        player.state = PlayerState::Dead;
        return;
    }

    player.hitInvulnTicks = player.stats.hitInvulnTicks;
    player.knockback = awayDir * kKnockbackSpeed;
    if (player.state == PlayerState::Attacking) player.state = PlayerState::Normal;  // interrupted
}

bool isAttackActive(const Player& player) {
    if (player.state != PlayerState::Attacking) return false;
    int elapsed = attackElapsed(player);
    return elapsed > kAttackStartupTicks && elapsed <= kAttackStartupTicks + kAttackActiveTicks;
}

Circle attackHitbox(const Player& player) {
    // Centered one radius ahead, so the swing covers from the player's own
    // position out to two radii in front.
    return {player.pos + player.facing * player.stats.attackRadius, player.stats.attackRadius};
}

void bufferPlayerInput(Player& player, const Input& input) {
    if (input.pressed(Action::Dash)) player.dashBufferTicks = kInputBufferTicks;
    if (input.pressed(Action::Attack)) player.attackBufferTicks = kInputBufferTicks;
}

void updatePlayer(Player& player, const Input& input, std::span<const Rect> walls, float dt) {
    player.prevPos = player.pos;
    if (player.state == PlayerState::Dead) return;

    if (player.dashCooldownTicks > 0) --player.dashCooldownTicks;
    if (player.dashBufferTicks > 0) --player.dashBufferTicks;
    if (player.attackBufferTicks > 0) --player.attackBufferTicks;
    if (player.hitInvulnTicks > 0) --player.hitInvulnTicks;
    if (player.hitFlashTicks > 0) --player.hitFlashTicks;
    bufferPlayerInput(player, input);

    // Facing is locked for the duration of a swing.
    Vec2 move = input.moveAxis();
    if (lengthSq(move) > 0.0f && player.state != PlayerState::Attacking) player.facing = move;

    Vec2 velocity;
    switch (player.state) {
    case PlayerState::Normal:
        if (canDash(player)) {
            startDash(player);
        } else if (player.attackBufferTicks > 0) {
            startAttack(player);
        } else {
            velocity = move * player.stats.moveSpeed;
        }
        break;

    case PlayerState::Dashing:
        velocity = player.dashDir * kDashSpeed;
        if (--player.stateTicks <= 0) player.state = PlayerState::Normal;
        break;

    case PlayerState::Attacking: {
        --player.stateTicks;
        bool inRecovery = attackElapsed(player) > kAttackStartupTicks + kAttackActiveTicks;
        if (inRecovery && canDash(player)) {
            startDash(player);  // dash-cancel
        } else if (player.stateTicks <= 0) {
            player.state = PlayerState::Normal;
        } else if (!inRecovery) {
            velocity = player.facing * kAttackLungeSpeed;
        }
        break;
    }

    case PlayerState::Dead:
        break;
    }

    // Knockback rides on top of whatever the state is doing, then bleeds off.
    velocity += player.knockback;
    player.knockback = player.knockback * kKnockbackDecay;
    if (lengthSq(player.knockback) < 1.0f) player.knockback = {};

    player.pos = moveCircle(player.pos, player.radius, velocity * dt, walls);
}

void drawPlayer(Renderer& renderer, const Player& player, float alpha) {
    if (player.state == PlayerState::Dead) return;
    Vec2 p = lerp(player.prevPos, player.pos, alpha);

    // Swing: bright while the hitbox is live, fading out through recovery.
    if (player.state == PlayerState::Attacking) {
        int elapsed = attackElapsed(player);
        int sinceActive = elapsed - kAttackStartupTicks;
        uint8_t swingAlpha = 0;
        if (isAttackActive(player)) {
            swingAlpha = 150;
        } else if (sinceActive > kAttackActiveTicks && player.stats.attackRecoveryTicks > 0) {
            int fadeLeft = attackTotalTicks(player) - elapsed;
            swingAlpha = static_cast<uint8_t>(90 * fadeLeft / player.stats.attackRecoveryTicks);
        }
        if (swingAlpha > 0) {
            float radius = player.stats.attackRadius;
            renderer.drawCircle(p + player.facing * radius, radius, {255, 240, 200, swingAlpha});
        }
    }

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
