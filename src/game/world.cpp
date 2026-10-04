#include "game/world.h"

#include "engine/collision.h"

namespace {

constexpr float kWallThickness = 32.0f;
constexpr float kFloorTileSize = 64.0f;

constexpr float kEnemySpeed = 140.0f;  // px/s

// The enemy's touch hitbox extends this far beyond its solid body. Bodies are
// pushed apart every tick so they never overlap; the hitbox has to be larger
// than the body for contact to register at all.
constexpr float kContactHitboxPad = 4.0f;

void updateEnemies(World& world, float dt) {
    Player& player = world.player;

    // Chase: walk straight at the player. No pathfinding yet, so enemies get
    // stuck behind walls.
    for (Enemy& enemy : world.enemies) {
        enemy.prevPos = enemy.pos;

        Vec2 toPlayer = player.pos - enemy.pos;
        float reach = enemy.radius + player.radius;
        if (lengthSq(toPlayer) > reach * reach) {
            Vec2 delta = normalize(toPlayer) * (kEnemySpeed * dt);
            enemy.pos = moveCircle(enemy.pos, enemy.radius, delta, world.walls);
        }
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

// Enemy touch hitboxes vs. the player's hurtbox.
void resolveContactHits(World& world) {
    Player& player = world.player;
    if (isInvulnerable(player)) return;

    Circle hurtbox{player.pos, player.radius};
    for (const Enemy& enemy : world.enemies) {
        Circle hitbox{enemy.pos, enemy.radius + kContactHitboxPad};
        if (circlesOverlap(hitbox, hurtbox)) {
            hitPlayer(player);
            return;
        }
    }
}

Enemy makeEnemy(Vec2 pos) {
    Enemy enemy;
    enemy.pos = pos;
    enemy.prevPos = pos;
    return enemy;
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

    world.enemies.push_back(makeEnemy({200.0f, 150.0f}));
    world.enemies.push_back(makeEnemy({1000.0f, 150.0f}));
    world.enemies.push_back(makeEnemy({1080.0f, 560.0f}));

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
    updatePlayer(world.player, input, world.walls, dt);
    updateEnemies(world, dt);
    resolveContactHits(world);
}

void drawWorld(Renderer& renderer, const WorldTextures& textures, const World& world,
               float alpha) {
    // Draw order is back to front. Consecutive quads sharing a texture batch
    // into one draw call: floor, then walls, then all the circles.

    // UVs beyond 1.0 make the repeat-wrapped texture tile; each texel is one tile.
    renderer.drawQuad(textures.floor, {0.0f, 0.0f, kRoomWidth, kRoomHeight},
                      {0.0f, 0.0f, kRoomWidth / (kFloorTileSize * 2.0f),
                       kRoomHeight / (kFloorTileSize * 2.0f)});

    for (const Rect& wall : world.walls) renderer.drawRect(wall, {86, 86, 110, 255});

    for (const Enemy& enemy : world.enemies) {
        renderer.drawCircle(lerp(enemy.prevPos, enemy.pos, alpha), enemy.radius,
                            {120, 200, 90, 255});
    }

    drawPlayer(renderer, world.player, alpha);
}
