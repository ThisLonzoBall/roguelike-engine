#include "game/ui.h"

#include <algorithm>

#include "engine/renderer.h"
#include "game/game.h"

namespace {

constexpr Color kTextColor{235, 230, 215, 255};
constexpr Color kDimTextColor{150, 150, 165, 255};
constexpr Color kAccentColor{250, 210, 90, 255};

constexpr float kMargin = 48.0f;

// Text is queued while the rectangles are drawn and drawn afterwards in one
// go. Rects and text use different textures, so interleaving them would start
// a new draw call at every switch; this keeps the whole UI to two.
struct TextItem {
    std::string text;
    Vec2 pos;
    float scale;
    Color color;
};

void addCentered(std::vector<TextItem>& texts, std::string text, float centerX, float y,
                 float scale, Color color) {
    float x = centerX - textWidth(text, scale) / 2.0f;
    texts.push_back({std::move(text), {x, y}, scale, color});
}

void drawHud(Renderer& renderer, const Game& game, std::vector<TextItem>& texts) {
    const Player& player = game.world.player;

    // Health pips, top-left.
    constexpr float kPip = 18.0f, kPipGap = 6.0f;
    for (int i = 0; i < player.stats.maxHp; ++i) {
        Rect pip{kMargin + static_cast<float>(i) * (kPip + kPipGap), kMargin, kPip, kPip};
        renderer.drawRect(pip, i < player.hp ? Color{220, 60, 60, 255} : Color{60, 40, 45, 255});
    }

    // Room number and name, top-right.
    std::string room = "Room " + std::to_string(game.depth) + ": " + currentRoom(game).name;
    float roomX = kRoomWidth - kMargin - textWidth(room, 2.0f);
    texts.push_back({std::move(room), {roomX, kMargin + 1.0f}, 2.0f, kTextColor});

    // Owned boons, bottom-left, one line per boon with its stack count.
    std::vector<std::string> lines;
    for (const std::string& name : game.ownedBoons) {
        bool listed = false;
        for (const std::string& line : lines) listed = listed || line == name;
        if (!listed) lines.push_back(name);
    }
    float y = kRoomHeight - kMargin - static_cast<float>(lines.size()) * kTextLineHeight * 2.0f;
    for (std::string& name : lines) {
        int stacks = boonStacks(game, name);
        if (stacks > 1) name += " x" + std::to_string(stacks);
        texts.push_back({std::move(name), {kMargin, y}, 2.0f, kDimTextColor});
        y += kTextLineHeight * 2.0f;
    }
}

void drawBoonChoice(Renderer& renderer, const Game& game, std::vector<TextItem>& texts) {
    constexpr float kCardW = 340.0f, kCardH = 250.0f, kCardGap = 30.0f, kCardY = 240.0f;
    constexpr float kPad = 20.0f, kBorder = 4.0f;
    constexpr float kBodyScale = 2.0f;
    const size_t bodyChars =
        static_cast<size_t>((kCardW - 2.0f * kPad) / (kTextCharWidth * kBodyScale));

    renderer.drawRect({0.0f, 0.0f, kRoomWidth, kRoomHeight}, {8, 8, 12, 190});
    addCentered(texts, "ROOM CLEARED", kRoomWidth / 2.0f, 110.0f, 5.0f, kAccentColor);
    addCentered(texts, "Choose a boon", kRoomWidth / 2.0f, 175.0f, 3.0f, kTextColor);

    const std::vector<BoonDef>& boons = game.scripts.boons();
    float count = static_cast<float>(game.offer.size());
    float startX = (kRoomWidth - (count * kCardW + (count - 1.0f) * kCardGap)) / 2.0f;

    for (size_t i = 0; i < game.offer.size(); ++i) {
        const BoonDef& boon = boons[static_cast<size_t>(game.offer[i])];
        bool selected = static_cast<int>(i) == game.cursor;
        float x = startX + static_cast<float>(i) * (kCardW + kCardGap);

        // Border (a larger rect behind), then the card face.
        renderer.drawRect({x - kBorder, kCardY - kBorder, kCardW + 2 * kBorder, kCardH + 2 * kBorder},
                          selected ? kAccentColor : Color{70, 70, 90, 255});
        renderer.drawRect({x, kCardY, kCardW, kCardH},
                          selected ? Color{52, 50, 66, 255} : Color{32, 32, 44, 255});

        // Title: full size if it fits the card, otherwise the body size.
        float titleScale =
            textWidth(boon.name, 3.0f) <= kCardW - 2.0f * kPad ? 3.0f : kBodyScale;
        texts.push_back({boon.name, {x + kPad, kCardY + kPad}, titleScale,
                         selected ? kAccentColor : kTextColor});

        float lineY = kCardY + kPad + 44.0f;
        for (std::string& line : wrapText(boon.desc, bodyChars)) {
            texts.push_back({std::move(line), {x + kPad, lineY}, kBodyScale, kTextColor});
            lineY += kTextLineHeight * kBodyScale + 2.0f;
        }

        int stacks = boonStacks(game, boon.name);
        std::string owned = stacks == 0 ? "New"
                                        : "Owned " + std::to_string(stacks) + "/" +
                                              std::to_string(boon.maxStacks);
        texts.push_back({std::move(owned), {x + kPad, kCardY + kCardH - kPad - 16.0f}, kBodyScale,
                         kDimTextColor});
    }

    addCentered(texts, "A / D to choose     J or Space to take", kRoomWidth / 2.0f,
                kCardY + kCardH + 50.0f, 2.0f, kDimTextColor);
}

}  // namespace

std::vector<std::string> wrapText(std::string_view text, size_t maxChars) {
    std::vector<std::string> lines;
    if (maxChars == 0) return lines;

    std::string line;
    size_t pos = 0;
    while (pos < text.size()) {
        // Next word: skip spaces, then take up to the following space.
        while (pos < text.size() && text[pos] == ' ') ++pos;
        size_t end = text.find(' ', pos);
        if (end == std::string_view::npos) end = text.size();
        std::string_view word = text.substr(pos, end - pos);
        pos = end;
        if (word.empty()) break;

        // A word too long for any line is cut into line-sized pieces.
        while (word.size() > maxChars) {
            if (!line.empty()) {
                lines.push_back(std::move(line));
                line.clear();
            }
            lines.emplace_back(word.substr(0, maxChars));
            word.remove_prefix(maxChars);
        }
        if (word.empty()) continue;

        if (!line.empty() && line.size() + 1 + word.size() > maxChars) {
            lines.push_back(std::move(line));
            line.clear();
        }
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) lines.push_back(std::move(line));
    return lines;
}

void drawUi(Renderer& renderer, const Game& game) {
    std::vector<TextItem> texts;

    drawHud(renderer, game, texts);
    if (game.mode == GameMode::ChoosingBoon) drawBoonChoice(renderer, game, texts);

    for (const TextItem& item : texts) {
        renderer.drawText(item.text, item.pos, item.scale, item.color);
    }
}
