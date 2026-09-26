#pragma once

#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 v, float s) { return {v.x * s, v.y * s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }

inline float lengthSq(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline float length(Vec2 v) { return std::sqrt(lengthSq(v)); }

// Returns the zero vector for zero-length input instead of NaNs.
inline Vec2 normalize(Vec2 v) {
    float len = length(v);
    return len > 0.0f ? v * (1.0f / len) : Vec2{};
}

inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
