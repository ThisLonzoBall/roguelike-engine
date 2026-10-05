#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/random.h"
#include "game/room.h"
#include "game/world.h"

class Input;

// A run: a sequence of rooms played back to back until the player dies. Game
// owns what outlives a single room (the room pool, depth, the run's RNG) and
// builds a fresh World each time a room is entered.
struct Game {
    std::string roomDir;         // where .room files are loaded from
    std::vector<RoomDef> rooms;  // the pool; never empty
    Rng rng;                     // picks rooms and seeds each World

    int roomIndex = 0;           // index into `rooms` of the room being played
    int depth = 1;               // 1 for the first room of a run
    World world;
};

// Loads every .room file in `roomDir` and starts a run. Files that fail to
// parse are logged and skipped; if none load, a built-in room is used.
Game createGame(std::string roomDir, uint32_t seed);

// Re-reads the room files and restarts the current room, keeping depth and
// health. For editing rooms while the game is running.
void reloadRooms(Game& game);

// Advances the game by one fixed simulation tick, moving to the next room or
// restarting the run when the current room ends.
void updateGame(Game& game, const Input& input, float dt);

const RoomDef& currentRoom(const Game& game);
