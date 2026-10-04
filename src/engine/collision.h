#pragma once

#include <span>

#include "engine/math.h"

// Collision primitives. Characters are circles, static level geometry is
// axis-aligned rectangles. Everything here is "overlap, then push out":
// there is no velocity or physics response, just positional correction.

struct Circle {
    Vec2 center;
    float radius = 0.0f;
};

bool circlesOverlap(Circle a, Circle b);

// If the circle overlaps the rect, moves `center` out along the shortest path
// and returns true.
bool resolveCircleRect(Vec2& center, float radius, const Rect& rect);

// Moves a circle by `delta`, colliding with `walls`, and returns the new
// position. Long moves are split into sub-steps shorter than the radius so a
// fast mover (e.g. a dash) can't tunnel through thin walls. Sliding along
// walls falls out of the push-out for free.
Vec2 moveCircle(Vec2 pos, float radius, Vec2 delta, std::span<const Rect> walls);

// If the circles overlap, pushes them apart along the line between their
// centers and returns true. `aShare` (0..1) is the fraction of the correction
// applied to `a`; the rest goes to `b`. 0 makes `a` immovable.
bool separateCircles(Vec2& a, float radiusA, Vec2& b, float radiusB, float aShare);
