#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "input.h"
#include "player.h"

// --- Config ------------------------------------------------------------------
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// Simulation runs at a fixed rate regardless of frame rate, so gameplay
// (movement, hit timing, dash distance) is identical on every machine.
constexpr double kTickRate = 60.0;
constexpr double kDt = 1.0 / kTickRate;

// Cap on real time consumed per frame. Prevents a "spiral of death" after a
// long stall (breakpoint, window drag) where we'd try to catch up forever.
constexpr double kMaxFrameTime = 0.25;

// --- Entry point + game loop -------------------------------------------------
int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Roguelike", kWindowWidth, kWindowHeight, 0, &window,
                                     &renderer)) {
        SDL_Log("Window/renderer creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    Input input;
    Player player;
    player.pos = {kWindowWidth / 2.0f, kWindowHeight / 2.0f};
    player.prevPos = player.pos;

    Uint64 previousTime = SDL_GetTicksNS();
    double accumulator = 0.0;
    bool running = true;

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
            updatePlayer(player, input, static_cast<float>(kDt));
            input.consumePresses();
            accumulator -= kDt;
        }

        // 4. Draw, interpolating between the last two ticks
        float alpha = static_cast<float>(accumulator / kDt);
        SDL_SetRenderDrawColor(renderer, 20, 20, 28, 255);
        SDL_RenderClear(renderer);
        drawPlayer(renderer, player, alpha);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
