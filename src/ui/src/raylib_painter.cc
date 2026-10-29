#include <ui/backend/raylib_painter.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

using namespace ui;

namespace {

// Latin Extended-A covers file paths with accented user names.
std::vector<int> codepoints() {
    std::vector<int> result;
    for (int c = 32; c < 127; c++) {
        result.push_back(c);
    }
    for (int c = 160; c < 384; c++) {
        result.push_back(c);
    }
    for (int c :
         {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2192, 0x2212, 0x2248, 0x2264, 0x2265, 0x25B2,
          0x2713}) {
        result.push_back(c);
    }
    return result;
}

Vector2 vec(glm::vec2 v) {
    return {v.x, v.y};
}

// raylib culls clockwise triangles (y down), so order every triangle counter-clockwise.
float cross(glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

constexpr int kMaxSegments = 48;

int segmentsFor(float radius) {
    return std::clamp(static_cast<int>(radius * 1.5f), 6, kMaxSegments);
}

// Angles in degrees, y down, clockwise.
struct CornerArc {
    Corners corner;
    glm::vec2 centre;
    float startAngle;
};

std::array<CornerArc, 4> cornerArcs(const Rect &r, float radius) {
    const float l = static_cast<float>(r.left()), t = static_cast<float>(r.top());
    const float rr = static_cast<float>(r.right()), b = static_cast<float>(r.bottom());
    return {{
      {Corners::topLeft, {l + radius, t + radius}, 180.0f},
      {Corners::topRight, {rr - radius, t + radius}, 270.0f},
      {Corners::bottomRight, {rr - radius, b - radius}, 0.0f},
      {Corners::bottomLeft, {l + radius, b - radius}, 90.0f},
    }};
}

float clampRadius(const Rect &r, float radius) {
    return std::max(0.0f, std::min(radius, static_cast<float>(std::min(r.width(), r.height())) / 2.0f));
}

}  // namespace

RaylibPainter::RaylibPainter(float backingScale) : _scale(std::max(1.0f, backingScale)) {}

RaylibPainter::~RaylibPainter() {
    for (auto &slot : _fonts) {
        UnloadFont(slot.font);
    }
}

void RaylibPainter::load(FontSlot &slot) const {
    static const std::vector<int> glyphs = codepoints();
    slot.font = LoadFontFromMemory(
      ".ttf", slot.ttf.data(), static_cast<int>(slot.ttf.size()), static_cast<int>(std::round(slot.size * _scale)),
      const_cast<int *>(glyphs.data()), static_cast<int>(glyphs.size()));
    SetTextureFilter(slot.font.texture, TEXTURE_FILTER_BILINEAR);
}

int RaylibPainter::addFont(std::span<const uint8_t> ttf, float size, float lineHeight) {
    FontSlot slot{ttf, size, lineHeight, {}};
    load(slot);
    _fonts.push_back(slot);
    return static_cast<int>(_fonts.size()) - 1;
}

void RaylibPainter::setBackingScale(float scale) {
    scale = std::max(1.0f, scale);
    if (scale == _scale) {
        return;
    }
    _scale = scale;
    for (auto &slot : _fonts) {
        UnloadFont(slot.font);
        load(slot);
    }
}

const RaylibPainter::FontSlot &RaylibPainter::slotAt(int index) const {
    return _fonts.at(index >= 0 && index < static_cast<int>(_fonts.size()) ? static_cast<size_t>(index) : 0);
}

void RaylibPainter::beginFrame() {
    _clips.clear();
    _layers.clear();
    _opacity = 1.0f;
    EndScissorMode();
}

::Color RaylibPainter::convert(Color c) const {
    return {c.r(), c.g(), c.b(), static_cast<unsigned char>(std::round(static_cast<float>(c.a()) * _opacity))};
}

const char *RaylibPainter::terminated(std::string_view utf8) const {
    _textBuffer.assign(utf8);
    return _textBuffer.c_str();
}

void RaylibPainter::applyClip() {
    if (_clips.empty()) {
        EndScissorMode();
        return;
    }
    const Rect &r = _clips.back();
    BeginScissorMode(r.left(), r.top(), std::max(0, r.width()), std::max(0, r.height()));
}

void RaylibPainter::pushClip(const Rect &r) {
    _clips.push_back(_clips.empty() ? r : _clips.back().intersect(r));
    applyClip();
}

void RaylibPainter::popClip() {
    if (!_clips.empty()) {
        _clips.pop_back();
    }
    applyClip();
}

void RaylibPainter::beginLayer(float opacity) {
    // Approximation: overlapping draws inside the layer show through each other while fading.
    _layers.push_back(_opacity);
    _opacity *= std::clamp(opacity, 0.0f, 1.0f);
}

void RaylibPainter::endLayer() {
    if (!_layers.empty()) {
        _opacity = _layers.back();
        _layers.pop_back();
    }
}

void RaylibPainter::fillRect(const Rect &r, Color color, float radius, Corners corners) {
    if (r.empty() || color.a() == 0) {
        return;
    }
    radius = clampRadius(r, radius);
    const ::Color c = convert(color);
    const Rectangle rec{
      static_cast<float>(r.left()), static_cast<float>(r.top()), static_cast<float>(r.width()),
      static_cast<float>(r.height())};
    if (radius <= 0.0f || corners == Corners::none) {
        DrawRectangleRec(rec, c);
        return;
    }
    if (corners == Corners::all) {
        const float roundness = radius * 2.0f / std::min(rec.width, rec.height);
        DrawRectangleRounded(rec, roundness, segmentsFor(radius), c);
        return;
    }
    DrawRectangleRec({rec.x + radius, rec.y, rec.width - 2 * radius, rec.height}, c);
    DrawRectangleRec({rec.x, rec.y + radius, radius, rec.height - 2 * radius}, c);
    DrawRectangleRec({rec.x + rec.width - radius, rec.y + radius, radius, rec.height - 2 * radius}, c);
    for (const auto &arc : cornerArcs(r, radius)) {
        if (hasCorner(corners, arc.corner)) {
            DrawCircleSector(vec(arc.centre), radius, arc.startAngle, arc.startAngle + 90.0f, segmentsFor(radius), c);
        } else {
            const float x = arc.centre.x < rec.x + rec.width / 2 ? rec.x : rec.x + rec.width - radius;
            const float y = arc.centre.y < rec.y + rec.height / 2 ? rec.y : rec.y + rec.height - radius;
            DrawRectangleRec({x, y, radius, radius}, c);
        }
    }
}

void RaylibPainter::drawRect(const Rect &r, Color color, float radius, float thickness, Corners corners) {
    if (r.empty() || color.a() == 0 || thickness <= 0.0f) {
        return;
    }
    radius = clampRadius(r, radius);
    const ::Color c = convert(color);
    const Rectangle rec{
      static_cast<float>(r.left()), static_cast<float>(r.top()), static_cast<float>(r.width()),
      static_cast<float>(r.height())};
    if (radius <= 0.0f || corners == Corners::none) {
        DrawRectangleLinesEx(rec, thickness, c);
        return;
    }
    if (corners == Corners::all) {
        const float roundness = radius * 2.0f / std::min(rec.width, rec.height);
        DrawRectangleRoundedLinesEx(rec, roundness, segmentsFor(radius), thickness, c);
        return;
    }
    const float t = thickness;
    const auto inset = [&](Corners corner) {
        return hasCorner(corners, corner) ? radius : 0.0f;
    };
    DrawRectangleRec(
      {rec.x + inset(Corners::topLeft), rec.y, rec.width - inset(Corners::topLeft) - inset(Corners::topRight), t}, c);
    DrawRectangleRec(
      {rec.x + inset(Corners::bottomLeft), rec.y + rec.height - t,
       rec.width - inset(Corners::bottomLeft) - inset(Corners::bottomRight), t},
      c);
    DrawRectangleRec(
      {rec.x, rec.y + std::max(inset(Corners::topLeft), t), t,
       rec.height - std::max(inset(Corners::topLeft), t) - std::max(inset(Corners::bottomLeft), t)},
      c);
    DrawRectangleRec(
      {rec.x + rec.width - t, rec.y + std::max(inset(Corners::topRight), t), t,
       rec.height - std::max(inset(Corners::topRight), t) - std::max(inset(Corners::bottomRight), t)},
      c);
    for (const auto &arc : cornerArcs(r, radius)) {
        if (hasCorner(corners, arc.corner)) {
            DrawRing(
              vec(arc.centre), std::max(0.0f, radius - t), radius, arc.startAngle, arc.startAngle + 90.0f,
              segmentsFor(radius), c);
        }
    }
}

void RaylibPainter::maskRoundedCorners(const Rect &r, float radius, Color color, Corners corners) {
    radius = clampRadius(r, radius);
    if (radius <= 0.0f || color.a() == 0) {
        return;
    }
    const int segments = segmentsFor(radius);
    for (const auto &arc : cornerArcs(r, radius)) {
        if (!hasCorner(corners, arc.corner)) {
            continue;
        }
        // Fan from the square corner over the arc: exactly the area outside the curve.
        const float mid = (arc.startAngle + 45.0f) * std::numbers::pi_v<float> / 180.0f;
        std::array<glm::vec2, kMaxSegments + 2> fan;
        fan[0] = arc.centre + glm::vec2(std::cos(mid), std::sin(mid)) * (radius * std::numbers::sqrt2_v<float>);
        for (int i = 0; i <= segments; i++) {
            const float a = (arc.startAngle + 90.0f * static_cast<float>(i) / static_cast<float>(segments)) *
                            std::numbers::pi_v<float> / 180.0f;
            fan[static_cast<size_t>(i) + 1] = arc.centre + glm::vec2(std::cos(a), std::sin(a)) * radius;
        }
        fillConvex(std::span(fan.data(), static_cast<size_t>(segments) + 2), color);
    }
}

void RaylibPainter::drawLine(glm::vec2 from, glm::vec2 to, Color color, float thickness) {
    DrawLineEx(vec(from), vec(to), thickness, convert(color));
}

void RaylibPainter::fillTriangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, Color color) {
    if (cross(a, b, c) > 0.0f) {
        std::swap(b, c);
    }
    DrawTriangle(vec(a), vec(b), vec(c), convert(color));
}

void RaylibPainter::fillCircle(glm::vec2 center, float radius, Color color) {
    DrawCircleSector(vec(center), radius, 0.0f, 360.0f, segmentsFor(radius) * 2, convert(color));
}

void RaylibPainter::drawCircle(glm::vec2 center, float radius, Color color, float thickness) {
    DrawRing(vec(center), std::max(0.0f, radius - thickness), radius, 0.0f, 360.0f, segmentsFor(radius) * 2, convert(color));
}

void RaylibPainter::fillConvex(std::span<const glm::vec2> points, Color color) {
    if (points.size() < 3) {
        return;
    }
    for (size_t i = 1; i + 1 < points.size(); i++) {
        fillTriangle(points[0], points[i], points[i + 1], color);
    }
}

void RaylibPainter::strokeClosed(std::span<const glm::vec2> points, Color color, float thickness) {
    const ::Color c = convert(color);
    for (size_t i = 0; i < points.size(); i++) {
        const glm::vec2 a = points[i];
        const glm::vec2 b = points[(i + 1) % points.size()];
        DrawLineEx(vec(a), vec(b), thickness, c);
        DrawCircleSector(vec(a), thickness / 2.0f, 0.0f, 360.0f, 12, c);
    }
}

void RaylibPainter::drawText(std::string_view utf8, glm::ivec2 pos, Color color, int font) {
    if (utf8.empty() || _fonts.empty()) {
        return;
    }
    const auto &slot = slotAt(font);
    const float y = static_cast<float>(pos.y) + std::round((slot.lineHeight - slot.size) / 2.0f);
    DrawTextEx(slot.font, terminated(utf8), {static_cast<float>(pos.x), y}, slot.size, 0.0f, convert(color));
}

TextMetrics RaylibPainter::measureText(std::string_view utf8, int font) const {
    if (_fonts.empty()) {
        return {};
    }
    const auto &slot = slotAt(font);
    const Vector2 size = utf8.empty() ? Vector2{0, 0} : MeasureTextEx(slot.font, terminated(utf8), slot.size, 0.0f);
    return {{static_cast<int>(std::ceil(size.x)), static_cast<int>(std::ceil(slot.lineHeight))}};
}

int RaylibPainter::lineHeight(int font) const {
    return _fonts.empty() ? 0 : static_cast<int>(std::ceil(slotAt(font).lineHeight));
}
