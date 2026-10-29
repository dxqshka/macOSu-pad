#pragma once

#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "config.h"

namespace macosu_pad {

// Delay before moving so multi-finger gestures whose fingers land one by one aren't taken for pointing.
inline constexpr double kGestureTimeout = 0.02;

struct Point {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const Point &) const = default;
};

struct Mm {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const Mm &) const = default;
};

struct TipAnchor {
    double front = 0.0;
    double width = 0.0;
    TipDirection direction = TipDirection::up;
};

struct Touch {
    int32_t path = 0;
    double x = 0.0;
    double y = 0.0;
    // Contact ellipse axes in mm.
    double majorAxis = 0.0;
    double minorAxis = 0.0;
    // Radians, counter-clockwise with y up (MultitouchSupport's frame), in [0, π].
    double angle = 0.0;

    // `up` ignores the angle: for round contacts MultitouchSupport's angle snaps to 1.571 rad, jumping 0.6–0.9 mm/frame.
    Mm aimMm(Mm surface, TipAnchor anchor) const;
};

inline constexpr double kTipFadeAngle = 20.0 * std::numbers::pi / 180.0;

// Size is averaged because frame-to-frame ellipse changes made the cursor float ±3–4 px.
inline constexpr double kAimSizeTime = 1.0;

struct Mapping {
    Mapping(TrackpadArea area, DisplayArea display, Size screen);

    Mm local(Mm p) const;

    bool inside(Mm local) const;

    // The last screen row stays reachable so the Dock can appear.
    Point screenPoint(Mm local) const;

private:
    TrackpadArea _area;
    DisplayArea _display;
    double _cos;
    double _sin;
    // Display pixels per area mm.
    double _scaleX;
    double _scaleY;
    Size _screen;
};

struct Tracker {
    // `screen` in points, `surface` in mm.
    Tracker(const Config &config, Size screen, Size surface);

    // The first frames of every touch are held back until it is clear no gesture is starting.
    std::optional<Point> onFrame(std::span<const Touch> touches, double timestamp);

    struct AimSize {
        double majorAxis = 0.0;
        double minorAxis = 0.0;
    };
    std::optional<AimSize> aimSize() const {
        return _aimSize;
    }

private:
    enum struct GesturePhase {
        none,
        mayStart,
        began,
    };

    std::optional<Point> hold(size_t count);

    bool isGesturePath(int32_t path) const;

    Mapping _mapping;
    Mm _surface;
    TipAnchor _tipAnchor;
    bool _multitouchMovesCursor;
    // Screen points.
    std::optional<Point> _finger;
    std::optional<Point> _lastOutput;
    std::optional<int32_t> _activePath;
    std::optional<double> _touchStart;
    size_t _lastFingerCount = 1;
    GesturePhase _phase = GesturePhase::none;
    std::vector<int32_t> _gesturePaths;
    std::optional<AimSize> _aimSize;
    double _aimSizeTimestamp = 0.0;
};

}  // namespace macosu_pad
