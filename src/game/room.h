#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "engine/math.h"

// Every room is the same fixed grid of square tiles; the window scales the
// room to fit.
constexpr int kRoomCols = 32;
constexpr int kRoomRows = 18;
constexpr float kTileSize = 40.0f;
constexpr float kRoomWidth = kRoomCols * kTileSize;    // 1280
constexpr float kRoomHeight = kRoomRows * kTileSize;   // 720

// A room as authored in a .room file: static layout plus settings. This is
// read-only template data; the live state of a room being played is a World.
struct RoomDef {
    std::string name;
    int waves = 1;                    // enemy waves to clear before the doors open
    std::vector<Rect> walls;
    std::vector<Rect> doors;          // exits; solid until the room is cleared
    Vec2 playerStart;
    std::vector<Vec2> spawnPoints;    // where enemies may appear
};

// Parses the text of a .room file:
//
//   ; comment
//   name: Pillars
//   waves: 2
//   map:
//   ###############DD###############     32 columns x 18 rows
//   #..............................#     # wall    . floor
//   #..E.......P................E..#     P player start (exactly one)
//   ...                                  E enemy spawn point (at least one)
//                                        D door (at least one)
//
// Returns false and fills `error` (with a line number where it helps) if the
// text isn't a valid room. `out` is only modified on success.
bool parseRoom(std::string_view text, RoomDef& out, std::string& error);

// A room compiled into the executable, used when no room files can be loaded.
RoomDef fallbackRoom();
