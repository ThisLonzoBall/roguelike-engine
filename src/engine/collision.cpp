#include "engine/collision.h"

#include <algorithm>
#include <cmath>

bool circlesOverlap(Circle a, Circle b) {
    float r = a.radius + b.radius;
    return lengthSq(b.center - a.center) < r * r;
}

bool resolveCircleRect(Vec2& center, float radius, const Rect& rect) {
    float right = rect.x + rect.w;
    float bottom = rect.y + rect.h;

    // Closest point on the rect to the circle center.
    Vec2 closest{std::clamp(center.x, rect.x, right), std::clamp(center.y, rect.y, bottom)};
    Vec2 diff = center - closest;
    float distSq = lengthSq(diff);
    if (distSq >= radius * radius) return false;

    if (distSq > 0.0f) {
        // Center is outside the rect: push straight away from the closest point.
        float dist = std::sqrt(distSq);
        center += diff * ((radius - dist) / dist);
        return true;
    }

    // Center is inside the rect: exit through the nearest face.
    float toLeft = center.x - rect.x;
    float toRight = right - center.x;
    float toTop = center.y - rect.y;
    float toBottom = bottom - center.y;
    float nearest = std::min({toLeft, toRight, toTop, toBottom});

    if (nearest == toLeft) center.x = rect.x - radius;
    else if (nearest == toRight) center.x = right + radius;
    else if (nearest == toTop) center.y = rect.y - radius;
    else center.y = bottom + radius;
    return true;
}

Vec2 moveCircle(Vec2 pos, float radius, Vec2 delta, std::span<const Rect> walls) {
    constexpr float kMaxStepInRadii = 0.5f;

    float distance = length(delta);
    int steps = std::max(1, static_cast<int>(std::ceil(distance / (radius * kMaxStepInRadii))));
    Vec2 step = delta * (1.0f / static_cast<float>(steps));

    for (int i = 0; i < steps; ++i) {
        pos += step;
        for (const Rect& wall : walls) resolveCircleRect(pos, radius, wall);
    }
    return pos;
}

bool separateCircles(Vec2& a, float radiusA, Vec2& b, float radiusB, float aShare) {
    Vec2 diff = b - a;
    float minDist = radiusA + radiusB;
    float distSq = lengthSq(diff);
    if (distSq >= minDist * minDist) return false;

    // Exactly coincident centers have no direction; pick an arbitrary one.
    float dist = std::sqrt(distSq);
    Vec2 normal = dist > 0.0f ? diff * (1.0f / dist) : Vec2{1.0f, 0.0f};
    float overlap = minDist - dist;

    a += normal * (-overlap * aShare);
    b += normal * (overlap * (1.0f - aShare));
    return true;
}
