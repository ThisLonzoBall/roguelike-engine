#pragma once

// Minimal hand-rolled OpenGL 3.3 core loader. Only the types, constants and
// functions the engine actually uses are declared here; add to the list as
// needed. Functions are looked up at runtime via SDL_GL_GetProcAddress and
// called as gl::Clear(...), gl::DrawElements(...), etc.

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(_WIN64)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLfloat = float;
using GLchar = char;
using GLubyte = unsigned char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

// --- Constants ---------------------------------------------------------------
constexpr GLboolean GL_FALSE = 0;
constexpr GLboolean GL_TRUE = 1;

constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;

constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;

constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_UNSIGNED_INT = 0x1405;
constexpr GLenum GL_FLOAT = 0x1406;

constexpr GLenum GL_RENDERER = 0x1F01;
constexpr GLenum GL_VERSION = 0x1F02;

constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_REPEAT = 0x2901;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_RGBA8 = 0x8058;

constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;

constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;

// --- Functions: X(return type, name without "gl" prefix, (params)) ------------
#define GL_FUNCTION_LIST(X)                                                                  \
    X(void, Clear, (GLbitfield mask))                                                        \
    X(void, ClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                        \
    X(void, Viewport, (GLint x, GLint y, GLsizei width, GLsizei height))                     \
    X(void, Enable, (GLenum cap))                                                            \
    X(void, BlendFunc, (GLenum sfactor, GLenum dfactor))                                     \
    X(const GLubyte*, GetString, (GLenum name))                                              \
    X(void, GenTextures, (GLsizei n, GLuint* textures))                                      \
    X(void, DeleteTextures, (GLsizei n, const GLuint* textures))                             \
    X(void, BindTexture, (GLenum target, GLuint texture))                                    \
    X(void, ActiveTexture, (GLenum texture))                                                 \
    X(void, TexParameteri, (GLenum target, GLenum pname, GLint param))                       \
    X(void, TexImage2D, (GLenum target, GLint level, GLint internalFormat, GLsizei width,    \
                         GLsizei height, GLint border, GLenum format, GLenum type,           \
                         const void* pixels))                                                \
    X(GLuint, CreateShader, (GLenum type))                                                   \
    X(void, ShaderSource, (GLuint shader, GLsizei count, const GLchar* const* string,        \
                           const GLint* length))                                             \
    X(void, CompileShader, (GLuint shader))                                                  \
    X(void, GetShaderiv, (GLuint shader, GLenum pname, GLint* params))                       \
    X(void, GetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length,              \
                               GLchar* infoLog))                                             \
    X(void, DeleteShader, (GLuint shader))                                                   \
    X(GLuint, CreateProgram, ())                                                             \
    X(void, AttachShader, (GLuint program, GLuint shader))                                   \
    X(void, LinkProgram, (GLuint program))                                                   \
    X(void, GetProgramiv, (GLuint program, GLenum pname, GLint* params))                     \
    X(void, GetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length,            \
                                GLchar* infoLog))                                            \
    X(void, DeleteProgram, (GLuint program))                                                 \
    X(void, UseProgram, (GLuint program))                                                    \
    X(GLint, GetUniformLocation, (GLuint program, const GLchar* name))                       \
    X(void, Uniform1i, (GLint location, GLint v0))                                           \
    X(void, UniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose,           \
                               const GLfloat* value))                                        \
    X(void, GenVertexArrays, (GLsizei n, GLuint* arrays))                                    \
    X(void, DeleteVertexArrays, (GLsizei n, const GLuint* arrays))                           \
    X(void, BindVertexArray, (GLuint array))                                                 \
    X(void, GenBuffers, (GLsizei n, GLuint* buffers))                                        \
    X(void, DeleteBuffers, (GLsizei n, const GLuint* buffers))                               \
    X(void, BindBuffer, (GLenum target, GLuint buffer))                                      \
    X(void, BufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))    \
    X(void, BufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size,                 \
                            const void* data))                                               \
    X(void, EnableVertexAttribArray, (GLuint index))                                         \
    X(void, VertexAttribPointer, (GLuint index, GLint size, GLenum type,                     \
                                  GLboolean normalized, GLsizei stride,                      \
                                  const void* pointer))                                      \
    X(void, DrawElements, (GLenum mode, GLsizei count, GLenum type, const void* indices))

namespace gl {

#define GL_DECLARE_FUNCTION(ret, name, params) \
    using PFN_##name = ret(GLAPIENTRY*) params; \
    extern PFN_##name name;
GL_FUNCTION_LIST(GL_DECLARE_FUNCTION)
#undef GL_DECLARE_FUNCTION

// Resolves every function in the list. Requires a current GL context.
// Logs each missing function and returns false if any are missing.
bool load();

}  // namespace gl
