#pragma once

#include <span>
#include <string_view>
#include <utility>

#include <glm/glm.hpp>

#include <ui/core/color.h>
#include <ui/core/rect.h>

namespace ui {

enum struct Corners {
    none = 0,

    topLeft = 1 << 0,
    topRight = 1 << 1,
    bottomLeft = 1 << 2,
    bottomRight = 1 << 3,

    top = topLeft | topRight,
    bottom = bottomLeft | bottomRight,
    left = topLeft | bottomLeft,
    right = topRight | bottomRight,

    all = topLeft | topRight | bottomLeft | bottomRight,
};

constexpr bool hasCorner(Corners set, Corners corner) {
    return std::to_underlying(set) & std::to_underlying(corner);
}

struct TextMetrics {
    glm::ivec2 size{0, 0};
};

struct Painter {
    virtual ~Painter() = default;

    virtual void pushClip(const Rect &) = 0;
    virtual void popClip() = 0;

    virtual void beginLayer(float opacity) {
        (void)opacity;
    }
    virtual void endLayer() {}

    virtual void fillRect(const Rect &, Color, float radius = 0.0f, Corners = Corners::all) = 0;
    virtual void drawRect(const Rect &, Color, float radius = 0.0f, float thickness = 1.0f, Corners = Corners::all) = 0;

    // Paints `color` over the corners so square content underneath reads as rounded.
    virtual void maskRoundedCorners(const Rect &r, float radius, Color color, Corners corners = Corners::all) {
        (void)r;
        (void)radius;
        (void)color;
        (void)corners;
    }

    virtual void drawLine(glm::vec2 from, glm::vec2 to, Color, float thickness = 1.0f) {
        (void)from;
        (void)to;
        (void)thickness;
    }

    virtual void fillTriangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, Color) {
        (void)a;
        (void)b;
        (void)c;
    }

    virtual void fillCircle(glm::vec2 center, float radius, Color) {
        (void)center;
        (void)radius;
    }

    virtual void drawCircle(glm::vec2 center, float radius, Color, float thickness = 1.0f) {
        (void)center;
        (void)radius;
        (void)thickness;
    }

    virtual void fillConvex(std::span<const glm::vec2> points, Color) {
        (void)points;
    }

    virtual void strokeClosed(std::span<const glm::vec2> points, Color, float thickness = 1.0f) {
        (void)points;
        (void)thickness;
    }

    // `font` 0 = backend's primary font; `pos` is the top-left of the line box.
    virtual void drawText(std::string_view utf8, glm::ivec2 pos, Color, int font = 0) {
        (void)utf8;
        (void)pos;
        (void)font;
    }

    virtual TextMetrics measureText(std::string_view utf8, int font = 0) const {
        (void)utf8;
        (void)font;
        return {};
    }

    virtual int lineHeight(int font = 0) const {
        (void)font;
        return 0;
    }
};

// Used for layout-time measurement: Yoga measure callbacks run outside draw.
Painter *measurePainter();
void setMeasurePainter(Painter *);

}  // namespace ui
