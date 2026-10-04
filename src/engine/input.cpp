#include "engine/input.h"

Input::Input()
    : bindings_{
          {SDL_SCANCODE_W, Action::MoveUp},
          {SDL_SCANCODE_UP, Action::MoveUp},
          {SDL_SCANCODE_S, Action::MoveDown},
          {SDL_SCANCODE_DOWN, Action::MoveDown},
          {SDL_SCANCODE_A, Action::MoveLeft},
          {SDL_SCANCODE_LEFT, Action::MoveLeft},
          {SDL_SCANCODE_D, Action::MoveRight},
          {SDL_SCANCODE_RIGHT, Action::MoveRight},
          {SDL_SCANCODE_SPACE, Action::Dash},
          {SDL_SCANCODE_LSHIFT, Action::Dash},
          {SDL_SCANCODE_J, Action::Attack},
          {SDL_SCANCODE_X, Action::Attack},
      } {}

void Input::handleEvent(const SDL_Event& event) {
    // Ignore OS key-repeat so holding a key doesn't count as repeated presses.
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;

    for (const Binding& binding : bindings_) {
        if (binding.key == event.key.scancode) pressed_[index(binding.action)] = true;
    }
}

void Input::sampleHeld() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    held_.fill(false);
    for (const Binding& binding : bindings_) {
        if (keys[binding.key]) held_[index(binding.action)] = true;
    }
}

void Input::consumePresses() { pressed_.fill(false); }

Vec2 Input::moveAxis() const {
    Vec2 dir;
    if (held(Action::MoveUp)) dir.y -= 1.0f;
    if (held(Action::MoveDown)) dir.y += 1.0f;
    if (held(Action::MoveLeft)) dir.x -= 1.0f;
    if (held(Action::MoveRight)) dir.x += 1.0f;
    return normalize(dir);
}
