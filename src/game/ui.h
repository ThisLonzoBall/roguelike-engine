#pragma once

#include <string>
#include <string_view>
#include <vector>

class Renderer;
struct Game;

// Draws everything that sits on top of the world: health, room name, owned
// boons, and the boon choice screen. Uses the same 1280x720 view units as the
// room. Call after the world has been drawn and the view origin reset, so the
// UI doesn't move with screen shake.
void drawUi(Renderer& renderer, const Game& game);

// Breaks `text` into lines of at most `maxChars` characters, at spaces where
// it can. A single word longer than a line is split.
std::vector<std::string> wrapText(std::string_view text, size_t maxChars);
