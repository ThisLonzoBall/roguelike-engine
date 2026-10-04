#include "game/world.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "engine/collision.h"

namespace {

constexpr float kWallThickness = 32.0f;
constexpr float kFloorTileSize = 64.0f;

// --- Tuning (durations in ticks, 60 per second) ------------------------------
constexpr float kEnemySpeed = 140.0f;  // px/s

// The enemy's touch hitbox extends this far beyond its solid body. Bodies are
// pushed apart every tick so they never overlap; the hitbox has to be larger
// than the body for contact to register at all.
constexpr float kContactHitboxPad = 4.0f;
constexpr int kContactDamage = 1;
constexpr int kAttackDamage = 1;

constexpr float kEnemyKnockbackSpeed = 520.0f;  // px/s initial -> ~48 px total
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

constexpr int kWaveDelayTicks = 90;
constexpr int kRespawnDelayTicks = 90;
constexpr int kMaxEnemiesPerWave = 8;
constexpr float kMinSpawnDistance = 250.0f;  // from the player

constexpr Vec2 kSpawnPoints[] = {
    {200.0f, 150.0f}, {1000.0f, 150.0f}, {1080.0f, 560.0f}, {160.0f, 600.0f},
    {720.0f, 90.0f},  {160.0f, 360.0f},  {1120.0f, 360.0f}, {560.0f, 620.0f},
};

constexpr Color kEnemyColor{120, 200, 90, 255};
constexpr Color kPlayerColor{230, 110, 60, 255};
constexpr Color kSparkColor{255, 240, 200, 255};

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

void spawnWave(World& world) {
    ++world.wave;
    int count = std::min(2 + world.wave, kMaxEnemiesPerWave);

    // Start at a random spawn point and walk the list, skipping any that are
    // too close to the player.
    const int pointCount = static_cast<int>(std::size(kSpawnPoints));
    int start = static_cast<int>(world.rng.next() % pointCount);
    for (int i = 0; i < pointCount && count > 0; ++i) {
        Vec2 point = kSpawnPoints[(start + i) % pointCount];
        if (length(point - world.player.pos) < kMinSpawnDistance) continue;

        Enemy enemy;
        enemy.pos = enemy.prevPos = point;
        world.enemies.push_back(enemy);
        spawnBurst(world, point, {}, 8, 160.0f, kEnemyColor);
        --count;
    }
}

// --- Simulation --------------------------------------------------------------

void updateEnemies(World& world, float dt) {
    Player& player = world.player;

    for (Enemy& enemy : world.enemies) {
        enemy.prevPos = enemy.pos;
        if (enemy.hitFlashTicks > 0) --enemy.hitFlashTicks;

        // Chase: walk straight at the player. No pathfinding yet, so enemies
        // get stuck behind walls. Stunned enemies only slide from knockback.
        Vec2 velocity = enemy.knockback;
        if (enemy.stunTicks > 0) {
            --enemy.stunTicks;
        } else {
            Vec2 toPlayer = player.pos - enemy.pos;
            float reach = enemy.radius + player.radius;
            if (lengthSq(toPlayer) > reach * reach) velocity += normalize(toPlayer) * kEnemySpeed;
        }

        enemy.knockback = enemy.knockback * kKnockbackDecay;
        if (lengthSq(enemy.knockback) < 1.0f) enemy.knockback = {};

        enemy.pos = moveCircle(enemy.pos, enemy.radius, velocity * dt, world.walls);
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
        for (const Rect& wall : world.walls) resolveCircleRect(enemy.pos, enemy.radius, wall);
    }
}

// The player's swing hitbox vs. enemy hurtboxes.
void resolvePlayerAttack(World& world) {
    Player& player = world.player;
    if (!isAttackActive(player)) return;

    Circle hitbox = attackHitbox(player);
    for (Enemy& enemy : world.enemies) {
        if (enemy.lastHitByAttack == player.attackId) continue;  // this swing already hit it
        if (!circlesOverlap(hitbox, {enemy.pos, enemy.radius})) continue;

        Vec2 away = normalize(enemy.pos - player.pos);
        if (lengthSq(away) == 0.0f) away = player.facing;

        enemy.lastHitByAttack = player.attackId;
        enemy.hp -= kAttackDamage;
        enemy.knockback = away * kEnemyKnockbackSpeed;
        enemy.stunTicks = kEnemyStunTicks;
        enemy.hitFlashTicks = kEnemyHitFlashTicks;

        if (enemy.hp <= 0) {
            spawnBurst(world, enemy.pos, {}, 18, 320.0f, kEnemyColor);
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
        if (enemy.stunTicks > 0) continue;
        Circle hitbox{enemy.pos, enemy.radius + kContactHitboxPad};
        if (!circlesOverlap(hitbox, hurtbox)) continue;

        Vec2 away = normalize(player.pos - enemy.pos);
        if (lengthSq(away) == 0.0f) away = -player.facing;

        hitPlayer(player, away, kContactDamage);
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

World createWorld() {
    World world;

    // Outer walls.
    world.walls.push_back({0.0f, 0.0f, kRoomWidth, kWallThickness});
    world.walls.push_back({0.0f, kRoomHeight - kWallThickness, kRoomWidth, kWallThickness});
    world.walls.push_back({0.0f, 0.0f, kWallThickness, kRoomHeight});
    world.walls.push_back({kRoomWidth - kWallThickness, 0.0f, kWallThickness, kRoomHeight});

    // Pillars.
    world.walls.push_back({400.0f, 250.0f, 96.0f, 96.0f});
    world.walls.push_back({800.0f, 400.0f, 160.0f, 64.0f});
    world.walls.push_back({600.0f, 120.0f, 48.0f, 160.0f});
    world.walls.push_back({250.0f, 500.0f, 200.0f, 16.0f});  // thin: exercises anti-tunnelling

    world.player.pos = {kRoomWidth / 2.0f, kRoomHeight / 2.0f};
    world.player.prevPos = world.player.pos;

    spawnWave(world);
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

void updateWorld(World& world, const Input& input, float dt) {
    world.trauma = std::max(0.0f, world.trauma - kTraumaDecayPerTick);

    // Hitstop: skip the whole simulation for a few ticks so the hit registers
    // visually. Presses made during the freeze are still buffered.
    if (world.hitstopTicks > 0) {
        --world.hitstopTicks;
        freezeInterpolation(world);
        bufferPlayerInput(world.player, input);
        return;
    }

    // Dead: let the effects play out, then start over.
    if (world.player.state == PlayerState::Dead) {
        freezeInterpolation(world);
        updateParticles(world, dt);
        if (--world.respawnTicks <= 0) world = createWorld();
        return;
    }

    updatePlayer(world.player, input, world.walls, dt);
    updateEnemies(world, dt);
    resolvePlayerAttack(world);
    resolveContactHits(world);
    updateParticles(world, dt);

    if (world.enemies.empty() && --world.waveDelayTicks <= 0) spawnWave(world);
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
    // every circle, then bars and HUD.

    // UVs beyond 1.0 make the repeat-wrapped texture tile; each texel is one tile.
    renderer.drawQuad(textures.floor, {0.0f, 0.0f, kRoomWidth, kRoomHeight},
                      {0.0f, 0.0f, kRoomWidth / (kFloorTileSize * 2.0f),
                       kRoomHeight / (kFloorTileSize * 2.0f)});

    for (const Rect& wall : world.walls) renderer.drawRect(wall, {86, 86, 110, 255});

    for (const Enemy& enemy : world.enemies) {
        Color color = enemy.hitFlashTicks > 0 ? Color{255, 255, 255, 255} : kEnemyColor;
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

    // HUD: player health pips.
    constexpr float kPip = 18.0f, kPipGap = 6.0f;
    for (int i = 0; i < world.player.maxHp; ++i) {
        Rect pip{48.0f + i * (kPip + kPipGap), 48.0f, kPip, kPip};
        renderer.drawRect(pip, i < world.player.hp ? Color{220, 60, 60, 255}
                                                   : Color{60, 40, 45, 255});
    }
}
