#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>

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

// --- Game state --------------------------------------------------------------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Player {
    Vec2 pos;              // position after the latest simulation tick
    Vec2 prevPos;          // position before it; used to interpolate rendering
    float speed = 300.0f;  // pixels per second
    float size = 32.0f;
};

// --- Update: advance the simulation by exactly one tick ----------------------
static void update(Player& player, const bool* keys, float dt) {
    player.prevPos = player.pos;

    Vec2 dir;
    if (keys[SDL_SCANCODE_W]) dir.y -= 1.0f;
    if (keys[SDL_SCANCODE_S]) dir.y += 1.0f;
    if (keys[SDL_SCANCODE_A]) dir.x -= 1.0f;
    if (keys[SDL_SCANCODE_D]) dir.x += 1.0f;

    // Normalize so diagonal movement isn't ~41% faster.
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 0.0f) {
        dir.x /= len;
        dir.y /= len;
    }

    player.pos.x += dir.x * player.speed * dt;
    player.pos.y += dir.y * player.speed * dt;
}

// --- Render: draw the current state, never modify it -------------------------
// alpha is how far we are between the previous and current tick (0..1).
static void render(SDL_Renderer* renderer, const Player& player, float alpha) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 28, 255);
    SDL_RenderClear(renderer);

    float x = player.prevPos.x + (player.pos.x - player.prevPos.x) * alpha;
    float y = player.prevPos.y + (player.pos.y - player.prevPos.y) * alpha;

    SDL_FRect rect{x - player.size / 2, y - player.size / 2, player.size, player.size};
    SDL_SetRenderDrawColor(renderer, 230, 110, 60, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);
}

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
        }

        // 2. Timing
        Uint64 now = SDL_GetTicksNS();
        double frameTime = static_cast<double>(now - previousTime) / 1e9;
        previousTime = now;
        if (frameTime > kMaxFrameTime) frameTime = kMaxFrameTime;
        accumulator += frameTime;

        // 3. Run as many fixed ticks as real time requires
        const bool* keys = SDL_GetKeyboardState(nullptr);
        while (accumulator >= kDt) {
            update(player, keys, static_cast<float>(kDt));
            accumulator -= kDt;
        }

        // 4. Draw, interpolating between the last two ticks
        render(renderer, player, static_cast<float>(accumulator / kDt));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
