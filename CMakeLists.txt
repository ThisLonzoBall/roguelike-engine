cmake_minimum_required(VERSION 3.24)
project(Roguelike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ------------------------------------------------------------
# SDL3 is downloaded and built from source on first configure, then linked
# statically so the game is a single .exe with no DLLs to copy around.
include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.16
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# Lua ships a Makefile rather than a CMake project, so fetch the source and
# build the library target here: everything in src/ except the two
# command-line programs (lua.c, luac.c).
FetchContent_Declare(
    lua
    URL      https://www.lua.org/ftp/lua-5.4.8.tar.gz
    URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM LUA_SOURCES ${lua_SOURCE_DIR}/src/lua.c ${lua_SOURCE_DIR}/src/luac.c)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${lua_SOURCE_DIR}/src)

# --- Game --------------------------------------------------------------------
# Engine code lives in src/engine/, game code in src/game/. Engine never
# includes game headers. Includes are written relative to src/.
add_executable(game
    src/main.cpp
    src/engine/collision.cpp
    src/engine/file.cpp
    src/engine/gl.cpp
    src/engine/input.cpp
    src/engine/renderer.cpp
    src/engine/script.cpp
    src/game/game.cpp
    src/game/player.cpp
    src/game/room.cpp
    src/game/scripts.cpp
    src/game/world.cpp
)
target_include_directories(game PRIVATE src)

# Development builds read data straight from the source tree, so rooms and
# scripts can be edited (and hot-reloaded with F5) without rebuilding or copying.
target_compile_definitions(game PRIVATE GAME_DEV_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
target_link_libraries(game PRIVATE SDL3::SDL3 lua)

if(MSVC)
    target_compile_options(game PRIVATE /W4)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT game)
else()
    target_compile_options(game PRIVATE -Wall -Wextra)
endif()
