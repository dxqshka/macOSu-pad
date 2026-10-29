#include "engine.h"

#include <algorithm>
#include <limits>
#include <mutex>

namespace macosu_pad {

namespace {

// Device family IDs below this are not trackpads.
constexpr int kMinTrackpadFamily = 98;
// Magic Mouse 1 & 2, and 3: multitouch surfaces that are not trackpads.
constexpr std::array kMagicMouseFamilies{112, 113};
// Used when the trackpad does not report its dimensions (mm).
constexpr Size kFallbackTrackpadMm{128.0, 80.0};
// `MTDeviceGetSensorSurfaceDimensions` reports hundredths of a millimetre.
constexpr double kSurfaceUnitsPerMm = 100.0;

// State shared with the MultitouchSupport callback thread.
struct Shared {
    std::optional<Tracker> tracker;
    bool moveCursor = false;
    std::unique_ptr<EventPoster> poster;
    std::vector<Touch> touches;
    Monitor monitor;
    MonitorFrame frame;
    double lastMonitorSend = -std::numeric_limits<double>::infinity();
};

std::mutex gSharedMutex;
Shared gShared;

Size surfaceMm(const mt::Api &api, mt::DeviceRef device) {
    int width = 0;
    int height = 0;
    api.getSensorSurfaceDimensions(device, &width, &height);
    if (width > 0 && height > 0) {
        return {width / kSurfaceUnitsPerMm, height / kSurfaceUnitsPerMm};
    }
    return kFallbackTrackpadMm;
}

bool needsAccessibility(const Config &config) {
    return config.emitMouseEvents || config.hideCursorInFullscreen;
}

std::expected<void, std::string> ensureAccessibility() {
    if (!accessibilityTrustedWithPrompt()) {
        return std::unexpected(
          "posting mouse events and hiding the cursor need Accessibility permission: allow this app "
          "in System Settings → Privacy & Security → Accessibility, then try again");
    }
    return {};
}

}  // namespace

Size mainDisplaySize() {
    const CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    return {bounds.size.width, bounds.size.height};
}

bool accessibilityTrustedWithPrompt() {
    const void *keys[] = {kAXTrustedCheckOptionPrompt};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef options =
      CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const bool trusted = AXIsProcessTrustedWithOptions(options);
    if (options) {
        CFRelease(options);
    }
    return trusted;
}

std::expected<std::unique_ptr<Engine>, std::string> Engine::create(Config config) {
    const char *loadError = nullptr;
    const mt::Api *api = mt::api(&loadError);
    if (!api) {
        return std::unexpected(loadError);
    }
    // Devices stay valid only while this owned CFArray is alive.
    CFArrayRef deviceList = api->createList();
    if (!deviceList) {
        return std::unexpected("MultitouchSupport returned no device list");
    }
    std::vector<mt::DeviceRef> trackpads;
    for (CFIndex i = 0, count = CFArrayGetCount(deviceList); i < count; i++) {
        const mt::DeviceRef device = CFArrayGetValueAtIndex(deviceList, i);
        int family = 0;
        api->getFamilyId(device, &family);
        if (family >= kMinTrackpadFamily && std::ranges::find(kMagicMouseFamilies, family) == kMagicMouseFamilies.end()) {
            api->registerContactFrameCallback(device, detail::frameCallback);
            trackpads.push_back(device);
        }
    }
    if (trackpads.empty()) {
        CFRelease(deviceList);
        return std::unexpected("no trackpad found");
    }
    const Size mm = surfaceMm(*api, trackpads.front());
    return std::unique_ptr<Engine>(new Engine(*api, deviceList, std::move(trackpads), mm, std::move(config)));
}

Engine::Engine(
  const mt::Api &api,
  CFArrayRef deviceList,
  std::vector<mt::DeviceRef> trackpads,
  Size trackpadMm,
  Config config)
    : _api(api),
      _deviceList(deviceList),
      _trackpads(std::move(trackpads)),
      _trackpadMm(trackpadMm),
      _config(std::move(config)) {
    resetTracker();
}

Engine::~Engine() {
    _tracking = false;
    _blocker.reset();
    _cursorHider.reset();
    setMonitor({});
    {
        std::lock_guard lock(gSharedMutex);
        gShared.moveCursor = false;
        gShared.poster.reset();
        gShared.tracker.reset();
    }
    CFRelease(_deviceList);
}

std::expected<void, std::string> Engine::setConfig(Config config) {
    if (_tracking && needsAccessibility(config)) {
        if (auto ok = ensureAccessibility(); !ok) {
            return ok;
        }
    }
    _config = std::move(config);
    resetTracker();
    return syncHelpers();
}

std::expected<void, std::string> Engine::startTracking() {
    if (needsAccessibility(_config)) {
        if (auto ok = ensureAccessibility(); !ok) {
            return ok;
        }
    }
    _tracking = true;
    resetTracker();
    syncDevices();
    return syncHelpers();
}

void Engine::stopTracking() {
    _tracking = false;
    {
        std::lock_guard lock(gSharedMutex);
        gShared.moveCursor = false;
    }
    _blocker.reset();
    _cursorHider.reset();
    syncDevices();
}

std::expected<void, std::string> Engine::syncHelpers() {
    const bool tracking = _tracking;
    if (!(tracking && _config.emitMouseEvents)) {
        _blocker.reset();
    }
    if (!(tracking && _config.hideCursorInFullscreen)) {
        _cursorHider.reset();
    }
    if (tracking && _config.emitMouseEvents && !_blocker) {
        auto blocker = MotionBlocker::start();
        if (!blocker) {
            stopTracking();
            return std::unexpected(std::move(blocker).error());
        }
        _blocker = std::move(*blocker);
    }
    if (tracking && _config.hideCursorInFullscreen && !_cursorHider) {
        _cursorHider = CursorHider::start();
    }
    return {};
}

void Engine::setMonitor(Monitor monitor) {
    {
        std::lock_guard lock(gSharedMutex);
        gShared.monitor = std::move(monitor);
        gShared.frame.fingerCount = 0;
        gShared.lastMonitorSend = -std::numeric_limits<double>::infinity();
    }
    syncDevices();
}

void Engine::resetTracker() {
    const Size screen = mainDisplaySize();
    auto poster = _config.emitMouseEvents ? std::make_unique<EventPoster>() : nullptr;
    std::unique_ptr<EventPoster> previous;
    {
        std::lock_guard lock(gSharedMutex);
        gShared.tracker.emplace(_config, screen, _trackpadMm);
        gShared.moveCursor = _tracking;
        previous = std::exchange(gShared.poster, std::move(poster));
        gShared.frame.cursor.reset();
    }
    // Drop the old poster (clearing its pinned click) before the new one posts.
    previous.reset();
}

void Engine::syncDevices() {
    bool wanted = _tracking;
    {
        std::lock_guard lock(gSharedMutex);
        wanted = wanted || static_cast<bool>(gShared.monitor);
    }
    if (wanted == _devicesRunning) {
        return;
    }
    for (const auto device : _trackpads) {
        if (wanted) {
            _api.start(device, 0);
        } else {
            _api.stop(device);
        }
    }
    _devicesRunning = wanted;
}

// ~125 Hz while a finger is down; everything here is allocation-free.
int detail::frameCallback(mt::DeviceRef, const mt::Touch *touches, int touchCount, double timestamp, int) {
    const std::span<const mt::Touch> raw =
      touches && touchCount > 0 ? std::span(touches, static_cast<size_t>(touchCount)) : std::span<const mt::Touch>();
    std::lock_guard lock(gSharedMutex);
    auto &shared = gShared;
    if (!shared.tracker) {
        return 0;
    }

    shared.touches.clear();
    for (const auto &t : raw) {
        if (t.state != mt::kMakeTouch && t.state != mt::kTouching) {
            continue;
        }
        shared.touches.push_back({
          .path = t.pathIndex,
          .x = t.normalized.position.x,
          // MultitouchSupport's origin is bottom-left; the screen's is top-left.
          .y = 1.0 - double(t.normalized.position.y),
          .majorAxis = t.majorAxis,
          .minorAxis = t.minorAxis,
          .angle = t.angle,
        });
    }
    if (const auto point = shared.tracker->onFrame(shared.touches, timestamp)) {
        if (shared.moveCursor) {
            if (shared.poster) {
                shared.poster->post(*point);
            } else {
                warp(*point);
            }
        }
        shared.frame.cursor = point;
    }

    if (shared.monitor) {
        const bool lifted = shared.touches.empty() && shared.frame.fingerCount != 0;
        if (lifted || timestamp - shared.lastMonitorSend >= kMonitorInterval) {
            auto &frame = shared.frame;
            frame.fingerCount = std::min(shared.touches.size(), kMaxMonitorTouches);
            for (size_t i = 0; i < frame.fingerCount; i++) {
                frame.fingers[i] = {static_cast<float>(shared.touches[i].x), static_cast<float>(shared.touches[i].y)};
            }
            if (shared.touches.size() == 1) {
                const auto &t = shared.touches.front();
                const auto aim = shared.tracker->aimSize().value_or(macosu_pad::Tracker::AimSize{t.majorAxis, t.minorAxis});
                frame.geometry = ContactGeometry{
                  static_cast<float>(t.majorAxis), static_cast<float>(t.minorAxis), static_cast<float>(t.angle),
                  static_cast<float>(aim.majorAxis), static_cast<float>(aim.minorAxis)};
            } else {
                frame.geometry.reset();
            }
            shared.lastMonitorSend = timestamp;
            shared.monitor(frame);
        }
    }
    return 0;
}

void detail::installTestTracker(std::optional<Tracker> tracker) {
    std::lock_guard lock(gSharedMutex);
    gShared.tracker = std::move(tracker);
    gShared.moveCursor = false;
    gShared.poster.reset();
    gShared.frame.cursor.reset();
}

std::optional<Point> detail::monitorCursor() {
    std::lock_guard lock(gSharedMutex);
    return gShared.frame.cursor;
}

}  // namespace macosu_pad
