#pragma once

// Everything about the player that boons can change. The defaults here are
// the base values at the start of a run; the Game recomputes the live values
// from these plus every boon the player owns (see Scripts::computeStats).
//
// Scripts see these under the snake_case names in the comments.
struct PlayerStats {
    int maxHp = 5;                    // max_hp
    float moveSpeed = 300.0f;         // move_speed       px/s
    int dashCooldownTicks = 18;       // dash_cooldown    ticks, measured from dash start
    int attackDamage = 1;             // attack_damage
    float attackRadius = 30.0f;       // attack_radius    px; also how far in front the swing is
    int attackRecoveryTicks = 9;      // attack_recovery  ticks after the swing before acting again
    float attackKnockback = 520.0f;   // knockback        px/s given to enemies that are hit
    int hitInvulnTicks = 45;          // mercy_ticks      invulnerability after taking a hit
    int healOnClear = 1;              // heal_on_clear    health restored when a room is cleared
};
