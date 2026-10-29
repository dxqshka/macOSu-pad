#include "tracker.h"

#include <algorithm>
#include <cmath>

namespace macosu_pad {

Mm Touch::aimMm(Mm surface, TipAnchor anchor) const {
    const double cx = x * surface.x;
    const double cy = y * surface.y;
    const double a = majorAxis / 2.0;
    const double b = minorAxis / 2.0;
    switch (anchor.direction) {
        case TipDirection::up:
            return {cx, cy - (anchor.front * std::sqrt(a * b) - anchor.width * b)};
        case TipDirection::finger: {
            const double sin = std::sin(angle);
            const double cos = std::cos(angle);
            const double fade = std::min(std::abs(sin) / std::sin(kTipFadeAngle), 1.0);
            // (cos, −sin) is the major axis in top-left coordinates, pointing up for angles in [0, π].
            const double shift = (anchor.front * a - anchor.width * b) * fade;
            return {cx + shift * cos, cy - shift * sin};
        }
    }
    return {cx, cy};
}

Mapping::Mapping(TrackpadArea area, DisplayArea display, Size screen)
    : _area(area),
      _display(display),
      _cos(std::cos(area.rotation * std::numbers::pi / 180.0)),
      _sin(std::sin(area.rotation * std::numbers::pi / 180.0)),
      _scaleX(display.width / area.width),
      _scaleY(display.height / area.height),
      _screen(screen) {}

Mm Mapping::local(Mm p) const {
    const double dx = p.x - _area.x;
    const double dy = p.y - _area.y;
    return {dx * _cos + dy * _sin, -dx * _sin + dy * _cos};
}

bool Mapping::inside(Mm local) const {
    return std::abs(local.x) <= _area.width / 2.0 && std::abs(local.y) <= _area.height / 2.0;
}

Point Mapping::screenPoint(Mm local) const {
    const double hw = _area.width / 2.0;
    const double hh = _area.height / 2.0;
    const double x = _display.x + std::clamp(local.x, -hw, hw) * _scaleX;
    const double y = _display.y + std::clamp(local.y, -hh, hh) * _scaleY;
    return {
      std::clamp(x, 0.0, std::max(_screen.width - 1.0, 0.0)),
      std::clamp(y, 0.0, std::max(_screen.height - 1.0, 0.0)),
    };
}

Tracker::Tracker(const Config &config, Size screen, Size surface)
    : _mapping(
        config.trackpadArea.value_or(TrackpadArea::full(surface)),
        config.displayArea.value_or(DisplayArea::full(screen)),
        screen),
      _surface{surface.width, surface.height},
      _tipAnchor{config.tipAnchor[0], config.tipAnchor[1], config.tipDirection},
      _multitouchMovesCursor(config.multitouchMovesCursor) {
    _gesturePaths.reserve(16);
}

bool Tracker::isGesturePath(int32_t path) const {
    return std::ranges::find(_gesturePaths, path) != _gesturePaths.end();
}

std::optional<Point> Tracker::onFrame(std::span<const Touch> touches, double timestamp) {
    const size_t count = touches.size();
    if (count == 0) {
        _gesturePaths.clear();
        _phase = GesturePhase::none;
        _lastFingerCount = 0;
        _touchStart.reset();
        _activePath.reset();
        return std::nullopt;
    }
    if (!_touchStart) {
        _touchStart = timestamp;
    }
    const bool withinTimeout = timestamp - *_touchStart < kGestureTimeout;

    if (_lastFingerCount != 1 && count == 1 && _phase == GesturePhase::none) {
        _phase = GesturePhase::mayStart;
        _lastFingerCount = count;
        return std::nullopt;
    }
    if (count == 1 && withinTimeout) {
        return std::nullopt;
    }
    if (count != 1 && (withinTimeout || _phase != GesturePhase::none)) {
        _phase = GesturePhase::began;
        for (const auto &t : touches) {
            if (!isGesturePath(t.path)) {
                _gesturePaths.push_back(t.path);
            }
        }
        _lastFingerCount = count;
        return hold(count);
    }
    // Fingers lift one by one at the end of a gesture: hold while any gesture finger remains.
    if (_phase == GesturePhase::began && std::ranges::any_of(touches, [this](const Touch &t) {
            return isGesturePath(t.path);
        })) {
        return hold(count);
    }
    _phase = GesturePhase::none;

    const auto it = std::ranges::find_if(touches, [this](const Touch &t) {
        return _activePath == t.path;
    });
    const Touch &touch = it != touches.end() ? *it : touches.front();
    AimSize size{touch.majorAxis, touch.minorAxis};
    if (_aimSize) {
        const double k = 1.0 - std::exp(-std::max(timestamp - _aimSizeTimestamp, 0.0) / kAimSizeTime);
        size.majorAxis = _aimSize->majorAxis + k * (touch.majorAxis - _aimSize->majorAxis);
        size.minorAxis = _aimSize->minorAxis + k * (touch.minorAxis - _aimSize->minorAxis);
    }
    Touch sized = touch;
    sized.majorAxis = size.majorAxis;
    sized.minorAxis = size.minorAxis;
    const Mm aim = sized.aimMm(_surface, _tipAnchor);
    // A finger landing outside the area is ignored; one sliding out is held at the edge.
    if (_activePath != touch.path) {
        if (!_mapping.inside(_mapping.local(aim))) {
            return hold(count);
        }
        _activePath = touch.path;
    }
    _aimSize = size;
    _aimSizeTimestamp = timestamp;
    _finger = _mapping.screenPoint(_mapping.local(aim));
    return hold(count);
}

std::optional<Point> Tracker::hold(size_t count) {
    if (count > 1 && !_multitouchMovesCursor) {
        return std::nullopt;
    }
    if (!_finger || _lastOutput == _finger) {
        return std::nullopt;
    }
    _lastOutput = _finger;
    return _finger;
}

}  // namespace macosu_pad
