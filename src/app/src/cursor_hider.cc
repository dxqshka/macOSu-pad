#include "cursor_hider.h"

#include <dlfcn.h>
#include <unistd.h>
#include <chrono>
#include <optional>
#include <utility>

namespace macosu_pad {

namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(100);
constexpr float kAxTimeoutSeconds = 0.1f;
constexpr double kEdgeTolerance = 1.0;

struct CfOwned {
    CFTypeRef ref = nullptr;

    CfOwned() = default;
    explicit CfOwned(CFTypeRef r) : ref(r) {}
    CfOwned(CfOwned &&other) noexcept : ref(std::exchange(other.ref, nullptr)) {}
    CfOwned &operator=(CfOwned &&) = delete;
    ~CfOwned() {
        if (ref) {
            CFRelease(ref);
        }
    }

    explicit operator bool() const {
        return ref != nullptr;
    }
};

CfOwned copyAttribute(CFTypeRef element, CFStringRef attribute) {
    CFTypeRef value = nullptr;
    const AXError status = AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(element), attribute, &value);
    return CfOwned(status == kAXErrorSuccess ? value : nullptr);
}

struct FocusedWindow {
    CfOwned systemWide{AXUIElementCreateSystemWide()};

    FocusedWindow() {
        if (systemWide) {
            AXUIElementSetMessagingTimeout(static_cast<AXUIElementRef>(systemWide.ref), kAxTimeoutSeconds);
        }
    }

    bool coversDisplay(pid_t ownPid, CGRect display) const {
        const auto app = copyAttribute(systemWide.ref, kAXFocusedApplicationAttribute);
        if (!app) {
            return false;
        }
        pid_t pid = 0;
        if (AXUIElementGetPid(static_cast<AXUIElementRef>(app.ref), &pid) != kAXErrorSuccess || pid == ownPid) {
            return false;
        }
        const auto window = copyAttribute(app.ref, kAXFocusedWindowAttribute);
        if (!window) {
            return false;
        }
        if (
          const auto fullScreen = copyAttribute(window.ref, CFSTR("AXFullScreen"));
          fullScreen && CFGetTypeID(fullScreen.ref) == CFBooleanGetTypeID() &&
          CFBooleanGetValue(static_cast<CFBooleanRef>(fullScreen.ref))) {
            return true;
        }
        const auto position = copyAttribute(window.ref, kAXPositionAttribute);
        const auto size = copyAttribute(window.ref, kAXSizeAttribute);
        CGRect frame{};
        return position && size &&
               AXValueGetValue(static_cast<AXValueRef>(position.ref), kAXValueTypeCGPoint, &frame.origin) &&
               AXValueGetValue(static_cast<AXValueRef>(size.ref), kAXValueTypeCGSize, &frame.size) && covers(frame, display);
    }
};

// Private SkyLight: "SetsCursorInBackground" lets a background app hide the cursor.
void allowBackgroundCursorChanges() {
    using DefaultConnection = int32_t (*)();
    using SetConnectionProperty = CGError (*)(int32_t, int32_t, CFStringRef, CFTypeRef);
    const auto defaultConnection = reinterpret_cast<DefaultConnection>(dlsym(RTLD_DEFAULT, "_CGSDefaultConnection"));
    const auto setProperty = reinterpret_cast<SetConnectionProperty>(dlsym(RTLD_DEFAULT, "CGSSetConnectionProperty"));
    if (defaultConnection && setProperty) {
        const int32_t connection = defaultConnection();
        setProperty(connection, connection, CFSTR("SetsCursorInBackground"), kCFBooleanTrue);
    }
}

}  // namespace

bool covers(CGRect w, CGRect d) {
    return w.origin.x <= d.origin.x + kEdgeTolerance && w.origin.y <= d.origin.y + kEdgeTolerance &&
           w.origin.x + w.size.width >= d.origin.x + d.size.width - kEdgeTolerance &&
           w.origin.y + w.size.height >= d.origin.y + d.size.height - kEdgeTolerance;
}

std::unique_ptr<CursorHider> CursorHider::start() {
    std::unique_ptr<CursorHider> hider(new CursorHider());
    hider->_thread = std::thread([self = hider.get()] {
        pthread_setname_np("cursor-hider");
        self->watch();
    });
    return hider;
}

CursorHider::~CursorHider() {
    {
        std::lock_guard lock(_mutex);
        _stop = true;
    }
    _wake.notify_all();
    if (_thread.joinable()) {
        _thread.join();
    }
}

void CursorHider::watch() {
    const FocusedWindow ax;
    if (!ax.systemWide) {
        return;
    }
    allowBackgroundCursorChanges();
    const pid_t ownPid = getpid();
    uint32_t hides = 0;
    std::unique_lock lock(_mutex);
    while (!_stop) {
        lock.unlock();
        const CGDirectDisplayID display = CGMainDisplayID();
        if (ax.coversDisplay(ownPid, CGDisplayBounds(display))) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            // Re-hide whenever macOS shows it anyway; deprecated CGCursorIsVisible still reports global visibility.
            if (hides == 0 || CGCursorIsVisible()) {
#pragma clang diagnostic pop
                CGDisplayHideCursor(display);
                hides++;
            }
        } else {
            for (; hides > 0; hides--) {
                CGDisplayShowCursor(display);
            }
        }
        lock.lock();
        _wake.wait_for(lock, kPollInterval, [this] {
            return _stop;
        });
    }
    lock.unlock();
    const CGDirectDisplayID display = CGMainDisplayID();
    for (; hides > 0; hides--) {
        CGDisplayShowCursor(display);
    }
}

}  // namespace macosu_pad
