#include "game/game.h"

#include <SDL3/SDL.h>

#include <utility>

#include "engine/file.h"

namespace {

std::vector<RoomDef> loadRooms(const std::string& roomDir) {
    std::vector<RoomDef> rooms;

    for (const std::string& path : listFiles(roomDir, ".room")) {
        std::optional<std::string> text = readTextFile(path);
        if (!text) {
            SDL_Log("Room %s: could not be read", path.c_str());
            continue;
        }

        RoomDef room;
        std::string error;
        if (!parseRoom(*text, room, error)) {
            SDL_Log("Room %s: %s", path.c_str(), error.c_str());
            continue;
        }
        if (room.name.empty()) room.name = path;
        rooms.push_back(std::move(room));
    }

    if (rooms.empty()) {
        SDL_Log("No rooms loaded from '%s'; using the built-in fallback room", roomDir.c_str());
        rooms.push_back(fallbackRoom());
    } else {
        SDL_Log("Loaded %d room(s) from '%s'", static_cast<int>(rooms.size()), roomDir.c_str());
    }
    return rooms;
}

// Random room, avoiding an immediate repeat when there's more than one.
int pickRoom(Game& game, int excludeIndex) {
    int count = static_cast<int>(game.rooms.size());
    if (count == 1) return 0;

    int index = static_cast<int>(game.rng.next() % static_cast<uint32_t>(count));
    if (index == excludeIndex) index = (index + 1) % count;
    return index;
}

void enterRoom(Game& game, int roomIndex, int playerHp) {
    game.roomIndex = roomIndex;
    game.world = createWorld(game.rooms[static_cast<size_t>(roomIndex)], game.depth, game.rng.next());
    game.world.player.hp = playerHp;
}

void startRun(Game& game) {
    game.depth = 1;
    enterRoom(game, pickRoom(game, -1), Player{}.maxHp);
}

}  // namespace

Game createGame(std::string roomDir, uint32_t seed) {
    Game game;
    game.roomDir = std::move(roomDir);
    game.rooms = loadRooms(game.roomDir);
    game.rng.state = seed != 0 ? seed : 1;  // xorshift can't start at zero
    startRun(game);
    return game;
}

void reloadRooms(Game& game) {
    // Stay in the same room if it still exists after the reload.
    std::string currentName = currentRoom(game).name;
    game.rooms = loadRooms(game.roomDir);

    int index = 0;
    for (size_t i = 0; i < game.rooms.size(); ++i) {
        if (game.rooms[i].name == currentName) index = static_cast<int>(i);
    }
    enterRoom(game, index, game.world.player.hp > 0 ? game.world.player.hp : Player{}.maxHp);
}

void updateGame(Game& game, const Input& input, float dt) {
    updateWorld(game.world, input, dt);

    switch (game.world.outcome) {
    case RoomOutcome::None:
        break;
    case RoomOutcome::Exited:
        ++game.depth;
        enterRoom(game, pickRoom(game, game.roomIndex), game.world.player.hp);
        break;
    case RoomOutcome::Died:
        startRun(game);
        break;
    }
}

const RoomDef& currentRoom(const Game& game) {
    return game.rooms[static_cast<size_t>(game.roomIndex)];
}
