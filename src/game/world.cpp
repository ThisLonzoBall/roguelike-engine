#include "game/world.h"

#include <algorithm>
#include <cmath>

#include "engine/collision.h"
#include "game/scripts.h"

namespace {

// --- Tuning (durations in ticks, 60 per second) ------------------------------
// The enemy's touch hitbox extends this far beyond its solid body. Bodies are
// pushed apart every tick so they never overlap; the hitbox has to be larger
// than the body for contact to register at all.
constexpr float kContactHitboxPad = 4.0f;

constexpr float kKnockbackDecay = 0.82f;        // per tick
constexpr int kEnemyStunTicks = 14;
constexpr int kEnemyHitFlashTicks = 6;

constexpr int kHitstopOnHit = 4;
constexpr int kHitstopOnKill = 6;
constexpr int kHitstopOnPlayerHit = 7;

constexpr float kTraumaOnHit = 0.25f;
constexpr float kTraumaOnKill = 0.45f;
constexpr float kTraumaOnPlayerHit = 0.6f;
constexpr float kTraumaDecayPerTick = 0.035f;
constexpr float kMaxShakePixels = 12.0f;

constexpr int kFirstWaveDelayTicks = 45;     // breathing room on entering
constexpr int kWaveDelayTicks = 90;
constexpr int kRespawnDelayTicks = 90;
constexpr int kMaxEnemiesPerWave = 8;
constexpr float kMinSpawnDistance = 250.0f;  // from the player
constexpr float kTraumaOnClear = 0.2f;

constexpr Color kPlayerColor{230, 110, 60, 255};
constexpr Color kSparkColor{255, 240, 200, 255};
constexpr Color kDoorLockedColor{120, 60, 50, 255};
constexpr Color kDoorOpenColor{250, 210, 90, 255};

// --- Effects -----------------------------------------------------------------

void addTrauma(World& world, float amount) {
    world.trauma = std::min(1.0f, world.trauma + amount);
}

void addHitstop(World& world, int ticks) {
    world.hitstopTicks = std::max(world.hitstopTicks, ticks);
}

// Sprays `count` particles outward from `pos`. If `dir` is non-zero the spray
// is a cone around it; otherwise it goes in all directions.
void spawnBurst(World& world, Vec2 pos, Vec2 dir, int count, float speed, Color color) {
    constexpr float kPi = 3.14159265f;
    bool directed = lengthSq(dir) > 0.0f;
    float baseAngle = directed ? std::atan2(dir.y, dir.x) : 0.0f;
    float spread = directed ? kPi * 0.35f : kPi;

    for (int i = 0; i < count; ++i) {
        float angle = baseAngle + world.rng.range(-spread, spread);
        Particle p;
        p.pos = p.prevPos = pos;
        p.vel = Vec2{std::cos(angle), std::sin(angle)} * (speed * world.rng.range(0.4f, 1.0f));
        p.radius = world.rng.range(2.0f, 4.5f);
        p.maxLifeTicks = p.lifeTicks = 14 + static_cast<int>(world.rng.next() % 14);
        p.color = color;
        world.particles.push_back(p);
    }
}

void updateParticles(World& world, float dt) {
    for (Particle& p : world.particles) {
        p.prevPos = p.pos;
        p.pos += p.vel * dt;
        p.vel = p.vel * 0.9f;
        --p.lifeTicks;
    }
    std::erase_if(world.particles, [](const Particle& p) { return p.lifeTicks <= 0; });
}

// While the simulation is frozen nothing moves, so collapse prevPos onto pos;
// otherwise render interpolation would keep replaying the last tick's motion.
void freezeInterpolation(World& world) {
    world.player.prevPos = world.player.pos;
    for (Enemy& enemy : world.enemies) enemy.prevPos = enemy.pos;
    for (Particle& p : world.particles) p.prevPos = p.pos;
}

// --- Spawning ----------------------------------------------------------------

void spawnEnemy(World& world, const Scripts& scripts, Vec2 point) {
    int typeIndex = scripts.pickEnemyType(world.depth, world.rng);
    const EnemyType& type = scripts.enemyTypes()[static_cast<size_t>(typeIndex)];

    Enemy enemy;
    enemy.type = typeIndex;
    enemy.id = world.nextEnemyId++;
    enemy.pos = enemy.prevPos = point;
    enemy.radius = type.radius;
    enemy.color = type.color;
    enemy.hp = enemy.maxHp = type.hp;
    enemy.contactDamage = type.contactDamage;
    world.enemies.push_back(enemy);
    spawnBurst(world, point, {}, 8, 160.0f, type.color);
}

void spawnWave(World& world, const Scripts& scripts) {
    --world.wavesRemaining;

    // One enemy per spawn point at most; deeper rooms use more of them.
    const int pointCount = static_cast<int>(world.spawnPoints.size());
    int count = std::min({2 + world.depth, kMaxEnemiesPerWave, pointCount});

    // Start at a random spawn point and walk the list. The first pass skips
    // points too close to the player; if that leaves the wave short (a
    // cramped room), a second pass fills in from the points that were skipped.
    int start = static_cast<int>(world.rng.next() % static_cast<uint32_t>(pointCount));
    std::vector<bool> used(world.spawnPoints.size(), false);
    for (int pass = 0; pass < 2 && count > 0; ++pass) {
        for (int i = 0; i < pointCount && count > 0; ++i) {
            size_t index = static_cast<size_t>((start + i) % pointCount);
            Vec2 point = world.spawnPoints[index];
            if (used[index]) continue;
            if (pass == 0 && length(point - world.player.pos) < kMinSpawnDistance) continue;

            spawnEnemy(world, scripts, point);
            used[index] = true;
            --count;
        }
    }
}

// --- Doors -------------------------------------------------------------------

void openDoors(World& world) {
    world.doorsOpen = true;
    world.justCleared = true;
    world.solids = world.walls;  // doors stop being solid

    Player& player = world.player;
    player.hp = std::min(player.stats.maxHp, player.hp + player.stats.healOnClear);

    for (const Rect& door : world.doors) {
        Vec2 center{door.x + door.w / 2.0f, door.y + door.h / 2.0f};
        spawnBurst(world, center, {}, 16, 260.0f, kDoorOpenColor);
    }
    addTrauma(world, kTraumaOnClear);
}

bool touchesAnyDoor(const World& world) {
    for (const Rect& door : world.doors) {
        Vec2 probe = world.player.pos;
        if (resolveCircleRect(probe, world.player.radius, door)) return true;
    }
    return false;
}

// --- Simulation --------------------------------------------------------------

void updateEnemies(World& world, Scripts& scripts, float dt) {
    Player& player = world.player;

    for (Enemy& enemy : world.enemies) {
        enemy.prevPos = enemy.pos;
        ++enemy.ageTicks;
        if (enemy.hitFlashTicks > 0) --enemy.hitFlashTicks;
        bool stunned = enemy.stunTicks > 0;
        if (stunned) --enemy.stunTicks;

        // The script decides where the enemy wants to go; the engine does the
        // rest. Think runs even while stunned (so scripts can react to being
        // hit), but its movement is ignored: stunned enemies only slide from
        // knockback.
        ThinkInput in;
        in.type = enemy.type;
        in.id = enemy.id;
        in.pos = enemy.pos;
        in.hp = enemy.hp;
        in.ageTicks = enemy.ageTicks;
        in.stunned = stunned;
        in.radius = enemy.radius;
        in.playerPos = player.pos;
        in.playerRadius = player.radius;
        in.dt = dt;
        in.depth = world.depth;
        ThinkResult thought = scripts.think(in, world.rng);
        enemy.hasTint = thought.hasTint;
        enemy.tint = thought.tint;

        Vec2 velocity = enemy.knockback + thought.velocity;

        enemy.knockback = enemy.knockback * kKnockbackDecay;
        if (lengthSq(enemy.knockback) < 1.0f) enemy.knockback = {};

        enemy.pos = moveCircle(enemy.pos, enemy.radius, velocity * dt, world.solids);
    }

    // Bodies are solid: push overlapping enemies apart, half each.
    for (size_t i = 0; i < world.enemies.size(); ++i) {
        for (size_t j = i + 1; j < world.enemies.size(); ++j) {
            Enemy& a = world.enemies[i];
            Enemy& b = world.enemies[j];
            separateCircles(a.pos, a.radius, b.pos, b.radius, 0.5f);
        }
    }

    // The player shoves enemies but isn't shoved back. Dashing passes through.
    if (player.state != PlayerState::Dashing) {
        for (Enemy& enemy : world.enemies) {
            separateCircles(player.pos, player.radius, enemy.pos, enemy.radius, 0.0f);
        }
    }

    // Separation may have pushed an enemy into a wall; walls win.
    for (Enemy& enemy : world.enemies) {
        for (const Rect& solid : world.solids) resolveCircleRect(enemy.pos, enemy.radius, solid);
    }
}

// The player's swing hitbox vs. enemy hurtboxes.
void resolvePlayerAttack(World& world, Scripts& scripts) {
    Player& player = world.player;
    if (!isAttackActive(player)) return;

    Circle hitbox = attackHitbox(player);
    for (Enemy& enemy : world.enemies) {
        if (enemy.lastHitByAttack == player.attackId) continue;  // this swing already hit it
        if (!circlesOverlap(hitbox, {enemy.pos, enemy.radius})) continue;

        Vec2 away = normalize(enemy.pos - player.pos);
        if (lengthSq(away) == 0.0f) away = player.facing;

        enemy.lastHitByAttack = player.attackId;
        enemy.hp -= player.stats.attackDamage;
        enemy.knockback = away * player.stats.attackKnockback;
        enemy.stunTicks = kEnemyStunTicks;
        enemy.hitFlashTicks = kEnemyHitFlashTicks;

        if (enemy.hp <= 0) {
            scripts.forgetEnemy(enemy.id);
            spawnBurst(world, enemy.pos, {}, 18, 320.0f, enemy.color);
            addHitstop(world, kHitstopOnKill);
            addTrauma(world, kTraumaOnKill);
        } else {
            spawnBurst(world, enemy.pos, away, 6, 260.0f, kSparkColor);
            addHitstop(world, kHitstopOnHit);
            addTrauma(world, kTraumaOnHit);
        }
    }

    bool hadEnemies = !world.enemies.empty();
    std::erase_if(world.enemies, [](const Enemy& enemy) { return enemy.hp <= 0; });
    if (hadEnemies && world.enemies.empty()) world.waveDelayTicks = kWaveDelayTicks;
}

// Enemy touch hitboxes vs. the player's hurtbox.
void resolveContactHits(World& world) {
    Player& player = world.player;
    if (isInvulnerable(player)) return;

    Circle hurtbox{player.pos, player.radius};
    for (const Enemy& enemy : world.enemies) {
        if (enemy.stunTicks > 0 || enemy.contactDamage <= 0) continue;
        Circle hitbox{enemy.pos, enemy.radius + kContactHitboxPad};
        if (!circlesOverlap(hitbox, hurtbox)) continue;

        Vec2 away = normalize(player.pos - enemy.pos);
        if (lengthSq(away) == 0.0f) away = -player.facing;

        hitPlayer(player, away, enemy.contactDamage);
        addHitstop(world, kHitstopOnPlayerHit);
        addTrauma(world, kTraumaOnPlayerHit);
        if (player.state == PlayerState::Dead) {
            spawnBurst(world, player.pos, {}, 28, 380.0f, kPlayerColor);
            world.respawnTicks = kRespawnDelayTicks;
        } else {
            spawnBurst(world, player.pos, away, 8, 240.0f, {255, 70, 70, 255});
        }
        return;
    }
}

}  // namespace

World createWorld(const RoomDef& room, int depth, uint32_t seed) {
    World world;
    world.walls = room.walls;
    world.doors = room.doors;
    world.spawnPoints = room.spawnPoints;
    world.depth = depth;

    // Doors start locked, so they're solid.
    world.solids = world.walls;
    world.solids.insert(world.solids.end(), world.doors.begin(), world.doors.end());

    world.player.pos = world.player.prevPos = room.playerStart;

    world.rng.state = seed != 0 ? seed : 1;  // xorshift can't start at zero
    world.wavesRemaining = room.waves;
    world.waveDelayTicks = kFirstWaveDelayTicks;
    return world;
}

WorldTextures createWorldTextures() {
    // 2x2 checkerboard, repeated across the room as a placeholder floor.
    const uint8_t floorPixels[] = {
        38, 38, 50, 255,  30, 30, 40, 255,
        30, 30, 40, 255,  38, 38, 50, 255,
    };

    WorldTextures textures;
    textures.floor =
        Texture::fromPixels(2, 2, floorPixels, TextureFilter::Nearest, TextureWrap::Repeat);
    return textures;
}

void updateWorld(World& world, const Input& input, Scripts& scripts, float dt) {
    world.trauma = std::max(0.0f, world.trauma - kTraumaDecayPerTick);

    // Hitstop: skip the whole simulation for a few ticks so the hit registers
    // visually. Presses made during the freeze are still buffered.
    if (world.hitstopTicks > 0) {
        --world.hitstopTicks;
        freezeInterpolation(world);
        bufferPlayerInput(world.player, input);
        return;
    }

    // Dead: let the effects play out, then report it.
    if (world.player.state == PlayerState::Dead) {
        freezeInterpolation(world);
        updateParticles(world, dt);
        if (--world.respawnTicks <= 0) world.outcome = RoomOutcome::Died;
        return;
    }

    updatePlayer(world.player, input, world.solids, dt);
    updateEnemies(world, scripts, dt);
    resolvePlayerAttack(world, scripts);
    resolveContactHits(world);
    updateParticles(world, dt);

    // Room flow: spawn waves until none are left, then open the doors.
    if (world.enemies.empty() && !world.doorsOpen) {
        if (world.wavesRemaining == 0) {
            openDoors(world);
        } else if (--world.waveDelayTicks <= 0) {
            spawnWave(world, scripts);
        }
    }
    if (world.doorsOpen && touchesAnyDoor(world)) world.outcome = RoomOutcome::Exited;
}

void idleWorld(World& world, float dt) {
    world.trauma = std::max(0.0f, world.trauma - kTraumaDecayPerTick);
    freezeInterpolation(world);
    updateParticles(world, dt);
}

Vec2 shakeOffset(const World& world, float timeSeconds) {
    // Squaring trauma makes small hits subtle and big hits violent. Two sines
    // at unrelated frequencies give a cheap jitter with no RNG, so shake never
    // touches simulation state.
    float amount = world.trauma * world.trauma * kMaxShakePixels;
    return {amount * std::sin(timeSeconds * 97.0f), amount * std::sin(timeSeconds * 113.0f + 1.3f)};
}

void drawWorld(Renderer& renderer, const WorldTextures& textures, const World& world,
               float alpha) {
    // Draw order is back to front. Consecutive quads sharing a texture batch
    // into one draw call, so each group below is one call: floor, walls,
    // every circle, then health bars.

    // UVs beyond 1.0 make the repeat-wrapped texture tile; each texel is one tile.
    renderer.drawQuad(textures.floor, {0.0f, 0.0f, kRoomWidth, kRoomHeight},
                      {0.0f, 0.0f, kRoomCols / 2.0f, kRoomRows / 2.0f});

    for (const Rect& wall : world.walls) renderer.drawRect(wall, {86, 86, 110, 255});
    for (const Rect& door : world.doors) {
        renderer.drawRect(door, world.doorsOpen ? kDoorOpenColor : kDoorLockedColor);
    }

    for (const Enemy& enemy : world.enemies) {
        Color color = enemy.hitFlashTicks > 0 ? Color{255, 255, 255, 255}
                      : enemy.hasTint         ? enemy.tint
                                              : enemy.color;
        renderer.drawCircle(lerp(enemy.prevPos, enemy.pos, alpha), enemy.radius, color);
    }

    drawPlayer(renderer, world.player, alpha);

    for (const Particle& p : world.particles) {
        float life = static_cast<float>(p.lifeTicks) / static_cast<float>(p.maxLifeTicks);
        Color color = p.color;
        color.a = static_cast<uint8_t>(255.0f * life);
        renderer.drawCircle(lerp(p.prevPos, p.pos, alpha), p.radius * life, color);
    }

    // Health bars over damaged enemies.
    for (const Enemy& enemy : world.enemies) {
        if (enemy.hp >= enemy.maxHp) continue;
        constexpr float kBarW = 28.0f, kBarH = 4.0f;
        Vec2 p = lerp(enemy.prevPos, enemy.pos, alpha);
        Rect bar{p.x - kBarW / 2, p.y - enemy.radius - 10.0f, kBarW, kBarH};
        renderer.drawRect(bar, {20, 20, 28, 220});
        bar.w = kBarW * static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp);
        renderer.drawRect(bar, {220, 60, 60, 255});
    }
}
