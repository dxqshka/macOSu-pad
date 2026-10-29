#pragma once

#include <array>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "config.h"
#include "cursor_hider.h"
#include "multitouch.h"
#include "output.h"
#include "tracker.h"

namespace macosu_pad {

inline constexpr size_t kMaxMonitorTouches = 16;
inline constexpr double kMonitorInterval = 1.0 / 60.0;

struct ContactGeometry {
    // Axes in millimetres.
    float majorAxis = 0.0f;
    float minorAxis = 0.0f;
    // Orientation of the major axis in radians, counter-clockwise with y up.
    float angle = 0.0f;
    float aimMajorAxis = 0.0f;
    float aimMinorAxis = 0.0f;
};

struct MonitorFrame {
    struct Finger {
        // Normalized, origin top-left.
        float x = 0.0f;
        float y = 0.0f;
    };

    std::array<Finger, kMaxMonitorTouches> fingers{};
    size_t fingerCount = 0;
    // Screen points; the cursor is only actually there while tracking.
    std::optional<Point> cursor;
    std::optional<ContactGeometry> geometry;

    std::span<const Finger> touches() const {
        return {fingers.data(), fingerCount};
    }
};

// Receives monitor frames on the MultitouchSupport thread; must not block.
using Monitor = std::function<void(const MonitorFrame &)>;

// Points (the "Looks like" resolution).
Size mainDisplaySize();

bool accessibilityTrustedWithPrompt();

struct Engine {
    static std::expected<std::unique_ptr<Engine>, std::string> create(Config config);

    ~Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    Size trackpadSizeMm() const {
        return _trackpadMm;
    }

    const Config &config() const {
        return _config;
    }

    bool isTracking() const {
        return _tracking;
    }

    std::expected<void, std::string> setConfig(Config config);

    std::expected<void, std::string> startTracking();

    void stopTracking();

    void setMonitor(Monitor monitor);

private:
    Engine(const mt::Api &api, CFArrayRef deviceList, std::vector<mt::DeviceRef> trackpads, Size trackpadMm, Config config);

    std::expected<void, std::string> syncHelpers();

    void resetTracker();

    void syncDevices();

    const mt::Api &_api;
    CFArrayRef _deviceList;
    std::vector<mt::DeviceRef> _trackpads;
    Size _trackpadMm;
    bool _devicesRunning = false;
    bool _tracking = false;
    Config _config;
    std::unique_ptr<MotionBlocker> _blocker;
    std::unique_ptr<CursorHider> _cursorHider;
};

namespace detail {

// Called by MultitouchSupport on its own thread for every trackpad frame.
int frameCallback(mt::DeviceRef, const mt::Touch *touches, int touchCount, double timestamp, int frame);

// For tests; never moves the real cursor.
void installTestTracker(std::optional<Tracker> tracker);

std::optional<Point> monitorCursor();

}  // namespace detail

}  // namespace macosu_pad
