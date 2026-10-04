#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>

#include "engine/gl.h"
#include "engine/input.h"
#include "engine/renderer.h"
#include "game/world.h"

// --- Config ------------------------------------------------------------------
// Simulation runs at a fixed rate regardless of frame rate, so gameplay
// (movement, hit timing, dash distance) is identical on every machine.
constexpr double kTickRate = 60.0;
constexpr double kDt = 1.0 / kTickRate;

// Cap on real time consumed per frame. Prevents a "spiral of death" after a
// long stall (breakpoint, window drag) where we'd try to catch up forever.
constexpr double kMaxFrameTime = 0.25;

// --- Game loop -----------------------------------------------------------------
// Separate from main() so every GL resource (renderer, textures) is destroyed
// before the GL context it belongs to.
static void run(SDL_Window* window) {
    Renderer renderer;
    if (!renderer.init()) return;

    WorldTextures textures = createWorldTextures();
    World world = createWorld();
    Input input;

    Uint64 previousTime = SDL_GetTicksNS();
    double accumulator = 0.0;
    bool running = true;

    // FPS / stats shown in the window title, refreshed once per second.
    Uint64 statsWindowStart = previousTime;
    int framesThisWindow = 0;

    while (running) {
        // 1. OS events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
            input.handleEvent(event);
        }
        input.sampleHeld();

        // 2. Timing
        Uint64 now = SDL_GetTicksNS();
        double frameTime = static_cast<double>(now - previousTime) / 1e9;
        previousTime = now;
        if (frameTime > kMaxFrameTime) frameTime = kMaxFrameTime;
        accumulator += frameTime;

        // 3. Run as many fixed ticks as real time requires
        while (accumulator >= kDt) {
            updateWorld(world, input, static_cast<float>(kDt));
            input.consumePresses();
            accumulator -= kDt;
        }

        // 4. Draw, interpolating between the last two ticks
        float alpha = static_cast<float>(accumulator / kDt);

        // Letterbox: scale the fixed-size room to the largest rectangle of the
        // same aspect ratio that fits the window, centered. The clear covers
        // the whole window, so the bars get the clear color.
        int pixelW = 0, pixelH = 0;
        SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);
        float scale = std::min(pixelW / kRoomWidth, pixelH / kRoomHeight);
        int viewportW = static_cast<int>(kRoomWidth * scale);
        int viewportH = static_cast<int>(kRoomHeight * scale);

        renderer.beginFrame((pixelW - viewportW) / 2, (pixelH - viewportH) / 2, viewportW,
                            viewportH, kRoomWidth, kRoomHeight);
        renderer.clear({12, 12, 16, 255});
        drawWorld(renderer, textures, world, alpha);

        renderer.endFrame();
        SDL_GL_SwapWindow(window);

        // 5. Stats
        ++framesThisWindow;
        if (now - statsWindowStart >= SDL_NS_PER_SECOND) {
            char title[128];
            std::snprintf(title, sizeof(title), "Roguelike | %d fps | %d draw calls, %d quads",
                          framesThisWindow, renderer.stats().drawCalls, renderer.stats().quads);
            SDL_SetWindowTitle(window, title);
            statsWindowStart = now;
            framesThisWindow = 0;
        }
    }
}

// --- Entry point -------------------------------------------------------------
int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Request an OpenGL 3.3 core context (forward-compatible is required on macOS).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Roguelike", static_cast<int>(kRoomWidth),
                                          static_cast<int>(kRoomHeight),
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                              SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        SDL_Log("OpenGL context creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);  // vsync

    if (gl::load()) {
        SDL_Log("OpenGL %s on %s", reinterpret_cast<const char*>(gl::GetString(GL_VERSION)),
                reinterpret_cast<const char*>(gl::GetString(GL_RENDERER)));
        run(window);
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
