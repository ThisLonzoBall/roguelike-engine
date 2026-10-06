#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/random.h"
#include "game/room.h"
#include "game/scripts.h"
#include "game/world.h"

class Input;

enum class GameMode {
    Playing,
    ChoosingBoon,  // room cleared; the world is frozen behind the choice screen
};

// A run: a sequence of rooms played back to back until the player dies. Game
// owns what outlives a single room (the room pool, scripts, depth, boons, the
// run's RNG) and builds a fresh World each time a room is entered.
struct Game {
    std::string assetDir;        // contains rooms/ (.room files) and scripts/ (.lua files)
    std::vector<RoomDef> rooms;  // the pool; never empty
    Scripts scripts;             // enemy types and behaviour
    Rng rng;                     // picks rooms and seeds each World

    int roomIndex = 0;           // index into `rooms` of the room being played
    int depth = 1;               // 1 for the first room of a run
    World world;

    // Boons. Owned boons are kept by name, one entry per stack in the order
    // they were picked, so they survive a script reload that reorders or
    // removes definitions.
    std::vector<std::string> ownedBoons;
    PlayerStats stats;           // base stats with every owned boon applied

    GameMode mode = GameMode::Playing;
    std::vector<int> offer;      // ChoosingBoon: indices into scripts.boons()
    int cursor = 0;              // ChoosingBoon: which offer is highlighted
    int menuLockTicks = 0;       // ChoosingBoon: input ignored while >0
};

// Loads the rooms and scripts under `assetDir` and starts a run. Files that
// fail to load are logged and skipped; built-in fallbacks cover the case where
// nothing loads.
Game createGame(std::string assetDir, uint32_t seed);

// Re-reads the room files and scripts and restarts the current room, keeping
// depth and health. For editing data while the game is running.
void reloadAssets(Game& game);

// Advances the game by one fixed simulation tick, moving to the next room or
// restarting the run when the current room ends.
void updateGame(Game& game, const Input& input, float dt);

const RoomDef& currentRoom(const Game& game);

// How many times the player has taken the named boon this run.
int boonStacks(const Game& game, std::string_view name);
