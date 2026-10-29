#include "overlay_shape.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace macosu_pad {

namespace {

constexpr int kEllipseSegments = 72;

// Trackpad mm → screen points: diag(dw/tw, dh/th) · [[cos θ, sin θ], [−sin θ, cos θ]], as {m00, m01, m10, m11}.
std::array<double, 4> mapping(const TrackpadArea &t, const DisplayArea &d) {
    const double gx = d.width / t.width;
    const double gy = d.height / t.height;
    const double r = t.rotation * std::numbers::pi / 180.0;
    return {gx * std::cos(r), gx * std::sin(r), -gy * std::sin(r), gy * std::cos(r)};
}

Vec2 mul(const std::array<double, 4> &m, Vec2 v) {
    return {m[0] * v.x + m[1] * v.y, m[2] * v.x + m[3] * v.y};
}

}  // namespace

std::optional<OverlayShape>
overlayShape(const Config &config, std::optional<ContactGeometry> geometry, std::optional<Point> cursor) {
    if (!geometry || !cursor || !config.trackpadArea || !config.displayArea) {
        return std::nullopt;
    }
    const auto m = mapping(*config.trackpadArea, *config.displayArea);
    const double a = geometry->majorAxis / 2.0;
    const double b = geometry->minorAxis / 2.0;
    const double angle = geometry->angle;

    // Mirrors `Touch::aimMm`.
    const auto tipVectors = [&](double a, double b) {
        if (config.tipDirection == TipDirection::up) {
            return std::pair{Vec2{0.0, -std::sqrt(a * b)}, Vec2{0.0, -b}};
        }
        const double sin = std::sin(angle);
        const double cos = std::cos(angle);
        const double fade = std::min(std::abs(sin) / std::sin(kTipFadeAngle), 1.0);
        return std::pair{Vec2{cos * fade * a, -sin * fade * a}, Vec2{cos * fade * b, -sin * fade * b}};
    };
    // The cursor was placed with the averaged size; the drawn ellipse is the live one.
    const auto [aimA, aimB] = tipVectors(geometry->aimMajorAxis / 2.0, geometry->aimMinorAxis / 2.0);
    const Vec2 A = tipVectors(a, b).first;
    const auto [front, width] = config.tipAnchor;
    const Vec2 shift = mul(m, {front * aimA.x - width * aimB.x, front * aimA.y - width * aimB.y});

    OverlayShape shape;
    shape.aim = {cursor->x, cursor->y};
    const Vec2 centre{cursor->x - shift.x, cursor->y - shift.y};

    // MultitouchSupport's angle is counter-clockwise with y up, so −angle with y down.
    const double cr = std::cos(-angle);
    const double sr = std::sin(-angle);
    shape.ellipse.reserve(kEllipseSegments);
    for (int i = 0; i < kEllipseSegments; i++) {
        const double t = 2.0 * std::numbers::pi * i / kEllipseSegments;
        const Vec2 local{a * std::cos(t) * cr - b * std::sin(t) * sr, a * std::cos(t) * sr + b * std::sin(t) * cr};
        const Vec2 v = mul(m, local);
        shape.ellipse.push_back({centre.x + v.x, centre.y + v.y});
    }

    const Vec2 edge = mul(m, A);
    shape.tipEdge = {centre.x + edge.x, centre.y + edge.y};

    shape.min = {shape.aim.x - kOverlayCrossHalf, shape.aim.y - kOverlayCrossHalf};
    shape.max = {shape.aim.x + kOverlayCrossHalf, shape.aim.y + kOverlayCrossHalf};
    const auto include = [&](Vec2 p, double r) {
        shape.min = {std::min(shape.min.x, p.x - r), std::min(shape.min.y, p.y - r)};
        shape.max = {std::max(shape.max.x, p.x + r), std::max(shape.max.y, p.y + r)};
    };
    include(shape.tipEdge, kOverlayDotRadius);
    for (const auto &p : shape.ellipse) {
        include(p, 0.0);
    }
    return shape;
}

}  // namespace macosu_pad
