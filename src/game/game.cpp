#include "game/game.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "engine/file.h"
#include "engine/input.h"

namespace {

constexpr int kBoonChoices = 3;

// The choice screen ignores input briefly after it opens, so the attack press
// that killed the last enemy doesn't also pick a boon.
constexpr int kMenuLockTicks = 24;

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
    game.scripts.forgetAllEnemies();  // the old World's enemies are gone
    game.roomIndex = roomIndex;
    game.mode = GameMode::Playing;
    game.world = createWorld(game.rooms[static_cast<size_t>(roomIndex)], game.depth, game.rng.next());
    game.world.player.stats = game.stats;
    game.world.player.hp = std::min(playerHp, game.stats.maxHp);
}

// Recomputes stats from the owned boons and hands them to the player. Gaining
// maximum health also heals by the amount gained.
void refreshStats(Game& game) {
    int oldMaxHp = game.stats.maxHp;
    game.stats = game.scripts.computeStats(PlayerStats{}, game.ownedBoons);

    Player& player = game.world.player;
    player.stats = game.stats;
    if (game.stats.maxHp > oldMaxHp) player.hp += game.stats.maxHp - oldMaxHp;
    player.hp = std::min(player.hp, game.stats.maxHp);
}

void startRun(Game& game) {
    game.depth = 1;
    game.ownedBoons.clear();
    game.stats = game.scripts.computeStats(PlayerStats{}, game.ownedBoons);
    enterRoom(game, pickRoom(game, -1), game.stats.maxHp);
}

// Picks up to kBoonChoices different boons the player can still take and
// opens the choice screen. Does nothing if there are none left.
void offerBoons(Game& game) {
    const std::vector<BoonDef>& boons = game.scripts.boons();

    std::vector<int> eligible;
    for (size_t i = 0; i < boons.size(); ++i) {
        if (boons[i].applyBroken) continue;
        if (boonStacks(game, boons[i].name) < boons[i].maxStacks) eligible.push_back(static_cast<int>(i));
    }
    if (eligible.empty()) return;

    // Partial Fisher-Yates shuffle: the first `count` slots end up a uniform
    // random sample without repeats.
    size_t count = std::min(eligible.size(), static_cast<size_t>(kBoonChoices));
    for (size_t i = 0; i < count; ++i) {
        size_t remaining = eligible.size() - i;
        size_t pick = i + game.rng.next() % static_cast<uint32_t>(remaining);
        std::swap(eligible[i], eligible[pick]);
    }
    eligible.resize(count);

    game.offer = std::move(eligible);
    game.cursor = static_cast<int>(count) / 2;  // start on the middle card
    game.menuLockTicks = kMenuLockTicks;
    game.mode = GameMode::ChoosingBoon;
}

void updateBoonChoice(Game& game, const Input& input) {
    if (game.menuLockTicks > 0) {
        --game.menuLockTicks;
        return;
    }

    int count = static_cast<int>(game.offer.size());
    if (input.pressed(Action::MoveLeft)) game.cursor = (game.cursor + count - 1) % count;
    if (input.pressed(Action::MoveRight)) game.cursor = (game.cursor + 1) % count;

    if (input.pressed(Action::Attack) || input.pressed(Action::Dash)) {
        int boonIndex = game.offer[static_cast<size_t>(game.cursor)];
        game.ownedBoons.push_back(game.scripts.boons()[static_cast<size_t>(boonIndex)].name);
        refreshStats(game);

        game.offer.clear();
        game.mode = GameMode::Playing;
    }
}

}  // namespace

Game createGame(std::string assetDir, uint32_t seed) {
    Game game;
    game.assetDir = std::move(assetDir);
    game.rooms = loadRooms(game.assetDir + "/rooms");
    game.scripts.load(game.assetDir + "/scripts");
    game.rng.state = seed != 0 ? seed : 1;  // xorshift can't start at zero
    startRun(game);
    return game;
}

void reloadAssets(Game& game) {
    // Stay in the same room if it still exists after the reload.
    std::string currentName = currentRoom(game).name;
    game.rooms = loadRooms(game.assetDir + "/rooms");
    game.scripts.load(game.assetDir + "/scripts");

    int index = 0;
    for (size_t i = 0; i < game.rooms.size(); ++i) {
        if (game.rooms[i].name == currentName) index = static_cast<int>(i);
    }

    // Boon definitions may have changed, so the stats may have too.
    int hp = game.world.player.hp;
    game.stats = game.scripts.computeStats(PlayerStats{}, game.ownedBoons);
    game.offer.clear();
    enterRoom(game, index, hp > 0 ? hp : game.stats.maxHp);
}

void updateGame(Game& game, const Input& input, float dt) {
    if (game.mode == GameMode::ChoosingBoon) {
        idleWorld(game.world, dt);
        updateBoonChoice(game, input);
        return;
    }

    updateWorld(game.world, input, game.scripts, dt);

    if (game.world.justCleared) {
        game.world.justCleared = false;
        offerBoons(game);
    }

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

int boonStacks(const Game& game, std::string_view name) {
    return static_cast<int>(std::count(game.ownedBoons.begin(), game.ownedBoons.end(), name));
}
