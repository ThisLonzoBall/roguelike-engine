#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "engine/gl.h"
#include "engine/math.h"

struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

enum class TextureFilter { Nearest, Linear };
enum class TextureWrap { Clamp, Repeat };

// GPU texture. Move-only; frees the GL texture on destruction, so it must be
// destroyed while the GL context is still alive.
class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // rgba: width * height * 4 bytes, rows top to bottom.
    static Texture fromPixels(int width, int height, const uint8_t* rgba,
                              TextureFilter filter = TextureFilter::Nearest,
                              TextureWrap wrap = TextureWrap::Clamp);

    GLuint id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    GLuint id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// Text is drawn with a built-in 8x8 monospace font. At `scale` 1 a character
// cell is 8 view units wide and a line is 10 tall.
constexpr float kTextCharWidth = 8.0f;
constexpr float kTextLineHeight = 10.0f;

// Width of the longest line of `text` at `scale`, in view units.
float textWidth(std::string_view text, float scale);

// 2D sprite batcher. Every draw call appends a textured quad to a CPU-side
// vertex buffer; the batch is uploaded and drawn in a single GL draw call
// when the texture changes, the buffer fills up, or the frame ends.
//
// Coordinates are in "view units" with (0,0) at the top-left and y down.
class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Requires a current GL context with functions loaded.
    bool init();

    // viewport: the region of the framebuffer to draw into, in pixels (GL
    // convention: origin bottom-left). view: size of the visible area in view
    // units, stretched to fill the viewport. viewOrigin: the view-space point
    // shown at the top-left corner (used for screen shake; a camera later).
    void beginFrame(int viewportX, int viewportY, int viewportW, int viewportH, float viewW,
                    float viewH, Vec2 viewOrigin = {});

    // Changes the view origin mid-frame. Everything drawn so far is flushed
    // with the old origin. Used to draw UI on top of a shaking world without
    // the UI shaking too.
    void setViewOrigin(Vec2 viewOrigin);

    void clear(Color color);
    void drawQuad(const Texture& texture, Rect dst, Rect uv, Color tint = {});
    void drawRect(Rect dst, Color color);
    void drawCircle(Vec2 center, float radius, Color color);

    // Draws printable ASCII with `pos` as the top-left corner. '\n' starts a
    // new line; other characters without a glyph are left blank.
    void drawText(std::string_view text, Vec2 pos, float scale, Color color);

    void endFrame();

    struct Stats {
        int drawCalls = 0;
        int quads = 0;
    };
    const Stats& stats() const { return stats_; }

private:
    struct Vertex {
        float x, y;
        float u, v;
        uint8_t r, g, b, a;
    };

    void flush();
    void applyProjection(Vec2 viewOrigin);

    float viewW_ = 1.0f;
    float viewH_ = 1.0f;

    GLuint program_ = 0;
    GLint projectionLoc_ = -1;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    Texture white_;
    Texture circle_;
    Texture font_;
    std::vector<Vertex> vertices_;
    GLuint batchTexture_ = 0;
    Stats stats_;
};
