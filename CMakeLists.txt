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

# --- Game --------------------------------------------------------------------
# Engine code lives in src/engine/, game code in src/game/. Engine never
# includes game headers. Includes are written relative to src/.
add_executable(game
    src/main.cpp
    src/engine/gl.cpp
    src/engine/input.cpp
    src/engine/renderer.cpp
    src/game/player.cpp
)
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE SDL3::SDL3)

if(MSVC)
    target_compile_options(game PRIVATE /W4)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT game)
else()
    target_compile_options(game PRIVATE -Wall -Wextra)
endif()
