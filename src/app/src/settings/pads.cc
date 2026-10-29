#include "pads.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace settings {

namespace {

constexpr int kStageInset = 6;
constexpr float kDotRadius = 7.0f;
constexpr float kCursorRadius = 6.0f;
// 45° stripes, 6 px clear then 6 px tinted.
constexpr float kStripePeriod = 12.0f;

glm::vec2 rotate(glm::vec2 v, float degrees) {
    const float r = degrees * std::numbers::pi_v<float> / 180.0f;
    const float c = std::cos(r), s = std::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

}  // namespace

ui::Rect AreaPad::padRect() const {
    const ui::Rect r = frame();
    const double aspect = _surface.width / _surface.height;
    const double availableWidth = std::max(0, r.width() - 2 * kStageInset);
    const double availableHeight = std::max(0, r.height() - 2 * kStageInset);
    const double width = std::max(0.0, std::min(availableWidth, availableHeight * aspect));
    const double height = width / aspect;
    const int w = static_cast<int>(std::round(width));
    const int h = static_cast<int>(std::round(height));
    const glm::ivec2 c = r.center();
    return {c.x - w / 2, c.y - h / 2, c.x - w / 2 + w, c.y - h / 2 + h};
}

double AreaPad::scale() const {
    return padRect().width() / _surface.width;
}

bool AreaPad::handleEvent(ui::Event &e) {
    switch (e.kind()) {
        case ui::EventKind::mouseDown: {
            auto &down = static_cast<ui::MouseButtonEvent &>(e);
            if (
              _disabled || down.button() != ui::MouseButton::left || padRect().width() <= 0 ||
              !areaContains(glm::vec2(down.position()) + 0.5f)) {
                return false;
            }
            _drag = Drag{down.position(), areaCentre(), _surface.width / padRect().width()};
            return true;
        }
        case ui::EventKind::mouseMove:
            if (_drag) {
                const glm::ivec2 delta = static_cast<ui::PointerEvent &>(e).position() - _drag->start;
                _drag->moved = true;
                if (onDrag) {
                    onDrag(_drag->centre.x + delta.x * _drag->unitsPerPx, _drag->centre.y + delta.y * _drag->unitsPerPx);
                }
                return true;
            }
            return false;
        case ui::EventKind::mouseUp:
            if (_drag) {
                const bool moved = _drag->moved;
                _drag.reset();
                if (onDragEnd) {
                    onDragEnd(moved);
                }
                return true;
            }
            return false;
        default:
            return false;
    }
}

void AreaPad::paintSurface(ui::Painter &p, float radius) {
    const auto &c = theme::colors();
    const ui::Rect pad = padRect();
    if (pad.empty()) {
        return;
    }
    p.fillRect(pad, c.pad, radius);
    p.pushClip(pad);
    const glm::vec2 g(std::numbers::sqrt2_v<float> / 2.0f, -std::numbers::sqrt2_v<float> / 2.0f);
    const glm::vec2 d(std::numbers::sqrt2_v<float> / 2.0f, std::numbers::sqrt2_v<float> / 2.0f);
    const glm::vec2 origin(static_cast<float>(pad.left()), static_cast<float>(pad.bottom()));
    const glm::vec2 size(static_cast<float>(pad.width()), static_cast<float>(pad.height()));
    const float extent = glm::dot(size, glm::abs(g)) + kStripePeriod;
    const float length = size.x + size.y;
    for (float s = kStripePeriod * 0.75f; s < extent; s += kStripePeriod) {
        const glm::vec2 mid = origin + g * s;
        p.drawLine(mid - d * length, mid + d * length, c.dead, kStripePeriod / 2.0f);
    }
    p.popClip();
    p.maskRoundedCorners(pad, radius, c.panel);
    p.drawRect(pad, c.border, radius, 1.0f);
}

DisplayPad::DisplayPad() {
    setStyle(ui::Style::empty().flexGrow(1.0f).flexShrink(1.0f).flexBasis(ui::Value::pt(0)).minHeight(ui::Value::pt(0)));
}

void DisplayPad::setCursor(std::optional<macosu_pad::Point> cursor, bool live) {
    if (cursor) {
        _cursor = cursor;
    }
    _live = live;
}

ui::Rect DisplayPad::areaRect() const {
    const ui::Rect pad = padRect();
    const double k = scale();
    const auto at = [&](double v, int origin) {
        return origin + static_cast<int>(std::round(v * k));
    };
    return {
      at(_area.x - _area.width / 2.0, pad.left()), at(_area.y - _area.height / 2.0, pad.top()),
      at(_area.x + _area.width / 2.0, pad.left()), at(_area.y + _area.height / 2.0, pad.top())};
}

bool DisplayPad::areaContains(glm::vec2 point) const {
    return areaRect().contains(glm::ivec2(point));
}

glm::dvec2 DisplayPad::areaCentre() const {
    return {_area.x, _area.y};
}

void DisplayPad::paint(ui::Painter &p, const ui::Rect &) {
    const auto &c = theme::colors();
    paintSurface(p, 4.0f);
    const ui::Rect pad = padRect();
    if (pad.empty()) {
        return;
    }
    const ui::Rect area = areaRect();
    p.fillRect(area, c.accentSoft, 3.0f);
    p.drawRect(area, c.accent, 3.0f, 2.0f);
    if (_cursor) {
        const glm::vec2 at(
          static_cast<float>(pad.left() + _cursor->x / _surface.width * pad.width()),
          static_cast<float>(pad.top() + _cursor->y / _surface.height * pad.height()));
        if (_live) {
            p.fillCircle(at, kCursorRadius + 5.0f, c.accentSoft);
            p.fillCircle(at, kCursorRadius, c.accent);
        } else {
            p.drawCircle(at, kCursorRadius, c.accent, 2.0f);
        }
    }
}

TrackpadPad::TrackpadPad() {
    setStyle(ui::Style::empty().flexGrow(1.0f).flexShrink(1.0f).flexBasis(ui::Value::pt(0)).minHeight(ui::Value::pt(0)));
}

void TrackpadPad::setTouches(std::span<const macosu_pad::MonitorFrame::Finger> touches) {
    _touches.assign(touches.begin(), touches.end());
}

std::array<glm::vec2, 4> TrackpadPad::corners(float inset) const {
    const ui::Rect pad = padRect();
    const float k = static_cast<float>(scale());
    const glm::vec2 centre(static_cast<float>(pad.left() + _area.x * k), static_cast<float>(pad.top() + _area.y * k));
    const float hw = static_cast<float>(_area.width) * k / 2.0f - inset;
    const float hh = static_cast<float>(_area.height) * k / 2.0f - inset;
    const auto r = static_cast<float>(_area.rotation);
    return {
      centre + rotate({-hw, -hh}, r), centre + rotate({hw, -hh}, r), centre + rotate({hw, hh}, r),
      centre + rotate({-hw, hh}, r)};
}

glm::vec2 TrackpadPad::toLocal(glm::vec2 point) const {
    const ui::Rect pad = padRect();
    const float k = static_cast<float>(scale());
    const glm::vec2 centre(static_cast<float>(pad.left() + _area.x * k), static_cast<float>(pad.top() + _area.y * k));
    return rotate(point - centre, -static_cast<float>(_area.rotation));
}

bool TrackpadPad::areaContains(glm::vec2 point) const {
    const glm::vec2 local = toLocal(point);
    const float k = static_cast<float>(scale());
    return std::abs(local.x) <= _area.width * k / 2.0f && std::abs(local.y) <= _area.height * k / 2.0f;
}

glm::dvec2 TrackpadPad::areaCentre() const {
    return {_area.x, _area.y};
}

void TrackpadPad::paint(ui::Painter &p, const ui::Rect &) {
    const auto &c = theme::colors();
    paintSurface(p, 10.0f);
    const ui::Rect pad = padRect();
    if (pad.empty()) {
        return;
    }
    const auto outline = corners(0.0f);
    p.fillConvex(outline, c.accentSoft);
    p.strokeClosed(corners(1.0f), c.accent, 2.0f);

    const float k = static_cast<float>(scale());
    const glm::vec2 centre(static_cast<float>(pad.left() + _area.x * k), static_cast<float>(pad.top() + _area.y * k));
    const float top = -static_cast<float>(_area.height) * k / 2.0f + 2.0f + 3.0f;
    const auto r = static_cast<float>(_area.rotation);
    p.fillTriangle(
      centre + rotate({0.0f, top}, r), centre + rotate({6.0f, top + 6.0f}, r), centre + rotate({-6.0f, top + 6.0f}, r),
      c.accent);

    const double cos = std::cos(_area.rotation * std::numbers::pi / 180.0);
    const double sin = std::sin(_area.rotation * std::numbers::pi / 180.0);
    for (const auto &touch : _touches) {
        const glm::vec2 at(
          static_cast<float>(pad.left() + touch.x * pad.width()), static_cast<float>(pad.top() + touch.y * pad.height()));
        const double px = touch.x * _surface.width - _area.x;
        const double py = touch.y * _surface.height - _area.y;
        const double lx = px * cos + py * sin;
        const double ly = -px * sin + py * cos;
        const bool inside = std::abs(lx) <= _area.width / 2.0 && std::abs(ly) <= _area.height / 2.0;
        if (inside) {
            p.fillCircle(at, kDotRadius + 4.0f, c.accentSoft);
            p.fillCircle(at, kDotRadius, c.accent);
        } else {
            p.fillCircle(at, kDotRadius, c.muted.withAlpha(static_cast<uint8_t>(0.8f * c.muted.a())));
        }
    }
}

}  // namespace settings
