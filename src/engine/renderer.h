#pragma once

#include <cstdint>
#include <vector>

#include "engine/gl.h"

struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
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

    // viewportPx: framebuffer size in pixels. view: size of the visible area in
    // view units (window size in logical points for now; a camera comes later).
    void beginFrame(int viewportPxW, int viewportPxH, float viewW, float viewH);
    void clear(Color color);
    void drawQuad(const Texture& texture, Rect dst, Rect uv, Color tint = {});
    void drawRect(Rect dst, Color color);
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

    GLuint program_ = 0;
    GLint projectionLoc_ = -1;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    Texture white_;
    std::vector<Vertex> vertices_;
    GLuint batchTexture_ = 0;
    Stats stats_;
};
