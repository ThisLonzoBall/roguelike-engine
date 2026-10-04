#include "engine/renderer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace {

constexpr int kMaxQuads = 10000;
constexpr int kVerticesPerQuad = 4;
constexpr int kIndicesPerQuad = 6;

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

const char* kFragmentShader = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main() {
    fragColor = texture(uTexture, vUV) * vColor;
}
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = gl::CreateShader(type);
    gl::ShaderSource(shader, 1, &source, nullptr);
    gl::CompileShader(shader);

    GLint ok = 0;
    gl::GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 0 ? len : 1), '\0');
        gl::GetShaderInfoLog(shader, len, nullptr, log.data());
        SDL_Log("Shader compile failed:\n%s", log.c_str());
        gl::DeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vsSource, const char* fsSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
    if (!vs || !fs) {
        if (vs) gl::DeleteShader(vs);
        if (fs) gl::DeleteShader(fs);
        return 0;
    }

    GLuint program = gl::CreateProgram();
    gl::AttachShader(program, vs);
    gl::AttachShader(program, fs);
    gl::LinkProgram(program);
    gl::DeleteShader(vs);  // flagged for deletion; freed when the program is
    gl::DeleteShader(fs);

    GLint ok = 0;
    gl::GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 0 ? len : 1), '\0');
        gl::GetProgramInfoLog(program, len, nullptr, log.data());
        SDL_Log("Shader link failed:\n%s", log.c_str());
        gl::DeleteProgram(program);
        return 0;
    }
    return program;
}

// White disc with an anti-aliased alpha edge, filling the whole texture.
// Tinted and scaled, it draws a circle of any size and color.
constexpr int kCircleTextureSize = 64;

Texture makeCircleTexture(int size) {
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    float center = size / 2.0f;
    float radius = center - 0.5f;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x + 0.5f - center;
            float dy = y + 0.5f - center;
            float dist = std::sqrt(dx * dx + dy * dy);
            float coverage = std::clamp(radius - dist + 0.5f, 0.0f, 1.0f);

            uint8_t* p = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = static_cast<uint8_t>(coverage * 255.0f + 0.5f);
        }
    }
    return Texture::fromPixels(size, size, pixels.data(), TextureFilter::Linear);
}

}  // namespace

// --- Texture -----------------------------------------------------------------

Texture::~Texture() {
    if (id_) gl::DeleteTextures(1, &id_);
}

Texture::Texture(Texture&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (id_) gl::DeleteTextures(1, &id_);
        id_ = std::exchange(other.id_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

Texture Texture::fromPixels(int width, int height, const uint8_t* rgba, TextureFilter filter,
                            TextureWrap wrap) {
    Texture tex;
    tex.width_ = width;
    tex.height_ = height;

    GLint glFilter = filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
    GLint glWrap = wrap == TextureWrap::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;

    gl::GenTextures(1, &tex.id_);
    gl::BindTexture(GL_TEXTURE_2D, tex.id_);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrap);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrap);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   rgba);
    return tex;
}

// --- Renderer ----------------------------------------------------------------

Renderer::~Renderer() {
    if (ebo_) gl::DeleteBuffers(1, &ebo_);
    if (vbo_) gl::DeleteBuffers(1, &vbo_);
    if (vao_) gl::DeleteVertexArrays(1, &vao_);
    if (program_) gl::DeleteProgram(program_);
}

bool Renderer::init() {
    program_ = linkProgram(kVertexShader, kFragmentShader);
    if (!program_) return false;
    projectionLoc_ = gl::GetUniformLocation(program_, "uProjection");
    gl::UseProgram(program_);
    gl::Uniform1i(gl::GetUniformLocation(program_, "uTexture"), 0);

    // The VAO records the vertex layout and the index buffer binding.
    gl::GenVertexArrays(1, &vao_);
    gl::BindVertexArray(vao_);

    // Vertex buffer: rewritten every batch, so allocate once at max size.
    gl::GenBuffers(1, &vbo_);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl::BufferData(GL_ARRAY_BUFFER, kMaxQuads * kVerticesPerQuad * sizeof(Vertex), nullptr,
                   GL_DYNAMIC_DRAW);

    constexpr GLsizei stride = sizeof(Vertex);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                            reinterpret_cast<const void*>(offsetof(Vertex, x)));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                            reinterpret_cast<const void*>(offsetof(Vertex, u)));
    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                            reinterpret_cast<const void*>(offsetof(Vertex, r)));

    // Index buffer: every quad uses the same pattern, so it's built once.
    std::vector<uint32_t> indices(kMaxQuads * kIndicesPerQuad);
    for (uint32_t q = 0; q < kMaxQuads; ++q) {
        uint32_t v = q * kVerticesPerQuad;
        uint32_t* i = &indices[q * kIndicesPerQuad];
        i[0] = v + 0; i[1] = v + 1; i[2] = v + 2;
        i[3] = v + 2; i[4] = v + 3; i[5] = v + 0;
    }
    gl::GenBuffers(1, &ebo_);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)), indices.data(),
                   GL_STATIC_DRAW);

    gl::BindVertexArray(0);

    // 1x1 white texture: solid-color rects are just quads tinted with this.
    const uint8_t white[4] = {255, 255, 255, 255};
    white_ = Texture::fromPixels(1, 1, white);
    circle_ = makeCircleTexture(kCircleTextureSize);

    vertices_.reserve(kMaxQuads * kVerticesPerQuad);

    gl::Enable(GL_BLEND);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void Renderer::beginFrame(int viewportX, int viewportY, int viewportW, int viewportH,
                          float viewW, float viewH) {
    stats_ = {};
    gl::Viewport(viewportX, viewportY, viewportW, viewportH);

    // Orthographic projection (column-major): maps x [0, viewW] -> [-1, 1] and
    // y [0, viewH] -> [1, -1], so the origin is the top-left corner, y down.
    const float projection[16] = {
        2.0f / viewW, 0.0f,          0.0f,  0.0f,
        0.0f,         -2.0f / viewH, 0.0f,  0.0f,
        0.0f,         0.0f,          -1.0f, 0.0f,
        -1.0f,        1.0f,          0.0f,  1.0f,
    };

    gl::UseProgram(program_);
    gl::UniformMatrix4fv(projectionLoc_, 1, GL_FALSE, projection);
    gl::BindVertexArray(vao_);
    gl::ActiveTexture(GL_TEXTURE0);
}

void Renderer::clear(Color c) {
    gl::ClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT);
}

void Renderer::drawQuad(const Texture& texture, Rect dst, Rect uv, Color tint) {
    if (texture.id() != batchTexture_ || vertices_.size() >= kMaxQuads * kVerticesPerQuad) {
        flush();
        batchTexture_ = texture.id();
    }

    float x0 = dst.x, y0 = dst.y, x1 = dst.x + dst.w, y1 = dst.y + dst.h;
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.w, v1 = uv.y + uv.h;
    uint8_t r = tint.r, g = tint.g, b = tint.b, a = tint.a;

    vertices_.push_back({x0, y0, u0, v0, r, g, b, a});  // top-left
    vertices_.push_back({x1, y0, u1, v0, r, g, b, a});  // top-right
    vertices_.push_back({x1, y1, u1, v1, r, g, b, a});  // bottom-right
    vertices_.push_back({x0, y1, u0, v1, r, g, b, a});  // bottom-left
}

void Renderer::drawRect(Rect dst, Color color) {
    drawQuad(white_, dst, {0.0f, 0.0f, 1.0f, 1.0f}, color);
}

void Renderer::drawCircle(Vec2 center, float radius, Color color) {
    drawQuad(circle_, {center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f},
             {0.0f, 0.0f, 1.0f, 1.0f}, color);
}

void Renderer::endFrame() { flush(); }

void Renderer::flush() {
    if (vertices_.empty()) return;

    // Orphan the old storage before writing so the driver can hand us fresh
    // memory instead of stalling until the previous draw has read it.
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl::BufferData(GL_ARRAY_BUFFER, kMaxQuads * kVerticesPerQuad * sizeof(Vertex), nullptr,
                   GL_DYNAMIC_DRAW);
    gl::BufferSubData(GL_ARRAY_BUFFER, 0,
                      static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                      vertices_.data());
    gl::BindTexture(GL_TEXTURE_2D, batchTexture_);

    GLsizei quadCount = static_cast<GLsizei>(vertices_.size() / kVerticesPerQuad);
    gl::DrawElements(GL_TRIANGLES, quadCount * kIndicesPerQuad, GL_UNSIGNED_INT, nullptr);

    stats_.drawCalls++;
    stats_.quads += quadCount;
    vertices_.clear();
}
