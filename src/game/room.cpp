#include "game/room.h"

#include <array>
#include <charconv>

namespace {

using Grid = std::array<std::array<char, kRoomCols>, kRoomRows>;

constexpr int kMaxWaves = 20;

std::string_view trim(std::string_view s) {
    const char* kSpace = " \t\r";
    size_t first = s.find_first_not_of(kSpace);
    if (first == std::string_view::npos) return {};
    size_t last = s.find_last_not_of(kSpace);
    return s.substr(first, last - first + 1);
}

// Pops the next line off the front of `text` (without its newline).
std::string_view nextLine(std::string_view& text) {
    size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return line;
}

Vec2 tileCenter(int col, int row) {
    return {(static_cast<float>(col) + 0.5f) * kTileSize,
            (static_cast<float>(row) + 0.5f) * kTileSize};
}

// Turns every tile equal to `tile` into rectangles, merging neighbours so a
// long wall becomes one box instead of dozens: each row is cut into
// horizontal runs, and a run is stacked onto the rect above it when that rect
// spans exactly the same columns. Fewer rects means fewer collision tests.
std::vector<Rect> mergeTiles(const Grid& grid, char tile) {
    std::vector<Rect> rects;
    std::vector<size_t> open;      // rects that reached the previous row
    std::vector<size_t> nextOpen;

    for (int row = 0; row < kRoomRows; ++row) {
        nextOpen.clear();
        for (int col = 0; col < kRoomCols;) {
            if (grid[row][col] != tile) {
                ++col;
                continue;
            }
            int runStart = col;
            while (col < kRoomCols && grid[row][col] == tile) ++col;
            float x = static_cast<float>(runStart) * kTileSize;
            float w = static_cast<float>(col - runStart) * kTileSize;

            bool extended = false;
            for (size_t index : open) {
                if (rects[index].x == x && rects[index].w == w) {
                    rects[index].h += kTileSize;
                    nextOpen.push_back(index);
                    extended = true;
                    break;
                }
            }
            if (!extended) {
                rects.push_back({x, static_cast<float>(row) * kTileSize, w, kTileSize});
                nextOpen.push_back(rects.size() - 1);
            }
        }
        open.swap(nextOpen);
    }
    return rects;
}

const char* kFallbackRoomText =
    "name: Fallback\n"
    "waves: 2\n"
    "map:\n"
    "###############DD###############\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..E........................E..#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..............................#\n"
    "#..E............P...........E..#\n"
    "#..............................#\n"
    "#..............................#\n"
    "################################\n";

}  // namespace

bool parseRoom(std::string_view text, RoomDef& out, std::string& error) {
    RoomDef room;
    int lineNumber = 0;
    auto fail = [&](const std::string& message) {
        error = "line " + std::to_string(lineNumber) + ": " + message;
        return false;
    };

    // --- Header: "key: value" lines up to "map:" ------------------------------
    bool foundMap = false;
    while (!text.empty() && !foundMap) {
        std::string_view line = trim(nextLine(text));
        ++lineNumber;
        if (line.empty() || line.front() == ';') continue;

        size_t colon = line.find(':');
        if (colon == std::string_view::npos) return fail("expected 'key: value'");
        std::string_view key = trim(line.substr(0, colon));
        std::string_view value = trim(line.substr(colon + 1));

        if (key == "map") {
            foundMap = true;
        } else if (key == "name") {
            room.name = std::string(value);
        } else if (key == "waves") {
            auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), room.waves);
            if (ec != std::errc{} || end != value.data() + value.size()) {
                return fail("'waves' must be a whole number");
            }
            if (room.waves < 1 || room.waves > kMaxWaves) {
                return fail("'waves' must be between 1 and " + std::to_string(kMaxWaves));
            }
        } else {
            return fail("unknown setting '" + std::string(key) + "'");
        }
    }
    if (!foundMap) {
        error = "missing 'map:' section";
        return false;
    }

    // --- Map: exactly kRoomRows lines of kRoomCols tiles -----------------------
    Grid grid{};
    int playerCount = 0;
    for (int row = 0; row < kRoomRows; ++row) {
        if (text.empty()) {
            error = "map has " + std::to_string(row) + " rows, expected " +
                    std::to_string(kRoomRows);
            return false;
        }
        std::string_view line = nextLine(text);
        ++lineNumber;
        if (static_cast<int>(line.size()) != kRoomCols) {
            return fail("map row is " + std::to_string(line.size()) + " tiles wide, expected " +
                        std::to_string(kRoomCols));
        }

        for (int col = 0; col < kRoomCols; ++col) {
            char tile = line[col];
            switch (tile) {
            case '#':
            case '.':
            case 'D':
                break;
            case 'P':
                room.playerStart = tileCenter(col, row);
                ++playerCount;
                break;
            case 'E':
                room.spawnPoints.push_back(tileCenter(col, row));
                break;
            default:
                return fail("unknown tile '" + std::string(1, tile) + "' at column " +
                            std::to_string(col + 1));
            }
            grid[row][col] = tile;
        }
    }
    while (!text.empty()) {
        ++lineNumber;
        if (!trim(nextLine(text)).empty()) return fail("unexpected text after the map");
    }

    room.walls = mergeTiles(grid, '#');
    room.doors = mergeTiles(grid, 'D');

    if (playerCount != 1) {
        error = "map needs exactly one player start 'P', found " + std::to_string(playerCount);
        return false;
    }
    if (room.spawnPoints.empty()) {
        error = "map needs at least one enemy spawn 'E'";
        return false;
    }
    if (room.doors.empty()) {
        error = "map needs at least one door 'D'";
        return false;
    }

    out = std::move(room);
    return true;
}

RoomDef fallbackRoom() {
    RoomDef room;
    std::string error;
    parseRoom(kFallbackRoomText, room, error);  // known-good text, so this can't fail
    return room;
}
