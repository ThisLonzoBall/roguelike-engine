#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <vector>

#include "math.h"

// Gameplay code asks about actions, never about physical keys. Bindings live
// in one table, which is where rebinding and gamepad support will plug in.
enum class Action {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Dash,
    Count,
};

class Input {
public:
    Input();

    // Call for every SDL event. Latches "pressed" edges as they happen so a
    // tap can't be lost between simulation ticks.
    void handleEvent(const SDL_Event& event);

    // Call once per frame before running ticks. Snapshots which actions are held.
    void sampleHeld();

    // Call after each simulation tick. A press is seen by exactly one tick;
    // if a frame runs zero ticks, presses carry over to the next frame.
    void consumePresses();

    bool held(Action action) const { return held_[index(action)]; }
    bool pressed(Action action) const { return pressed_[index(action)]; }

    // Normalized movement direction from the four move actions (zero if none).
    Vec2 moveAxis() const;

private:
    struct Binding {
        SDL_Scancode key;
        Action action;
    };

    static constexpr size_t kActionCount = static_cast<size_t>(Action::Count);
    static size_t index(Action action) { return static_cast<size_t>(action); }

    std::vector<Binding> bindings_;
    std::array<bool, kActionCount> held_{};
    std::array<bool, kActionCount> pressed_{};
};
