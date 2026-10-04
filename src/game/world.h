#pragma once

#include <vector>

#include "engine/math.h"
#include "engine/renderer.h"
#include "game/player.h"

class Input;

// The room is a fixed size in game units; the window scales it to fit.
constexpr float kRoomWidth = 1280.0f;
constexpr float kRoomHeight = 720.0f;

struct Enemy {
    Vec2 pos;
    Vec2 prevPos;          // position at the previous tick, for render interpolation
    float radius = 14.0f;  // collision body
};

// Everything that exists in the current room. Plain typed lists for now; this
// is the place a more general entity system would grow out of.
struct World {
    std::vector<Rect> walls;  // static, solid
    Player player;
    std::vector<Enemy> enemies;
};

// GPU resources the world is drawn with. Kept apart from World so simulation
// state stays plain data with no dependency on a GL context.
struct WorldTextures {
    Texture floor;
};

// Builds the hard-coded test room. Rooms will come from data files later.
World createWorld();
WorldTextures createWorldTextures();

// Advances the whole world by one fixed simulation tick.
void updateWorld(World& world, const Input& input, float dt);

// alpha: 0..1 blend between the previous and current tick.
void drawWorld(Renderer& renderer, const WorldTextures& textures, const World& world,
               float alpha);
