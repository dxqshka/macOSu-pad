#pragma once

#include <array>
#include <optional>
#include <vector>

#include "config.h"
#include "engine.h"

namespace macosu_pad {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct OverlayShape {
    // Screen points.
    std::vector<Vec2> ellipse;
    Vec2 tipEdge;
    Vec2 aim;
    // Bounding box without stroke widths.
    Vec2 min;
    Vec2 max;
};

// Points.
inline constexpr double kOverlayDotRadius = 3.0;
inline constexpr double kOverlayCrossHalf = 6.0;

std::optional<OverlayShape>
overlayShape(const Config &config, std::optional<ContactGeometry> geometry, std::optional<Point> cursor);

}  // namespace macosu_pad
