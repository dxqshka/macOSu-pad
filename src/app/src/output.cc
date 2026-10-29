#include "output.h"

#include <cmath>
#include <future>
#include <mutex>

namespace macosu_pad {

namespace {

std::mutex gLastPostedMutex;
std::optional<CGPoint> gLastPosted;

std::atomic<CFMachPortRef> gTapPort = nullptr;

std::optional<CGPoint> lastPosted() {
    std::lock_guard lock(gLastPostedMutex);
    return gLastPosted;
}

// Must be "dragged" while a button is held, or apps treat the drag as released.
std::pair<CGEventType, CGMouseButton> motionEventKind() {
    const auto pressed = [](CGMouseButton button) {
        return CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, button);
    };
    if (pressed(kCGMouseButtonLeft)) {
        return {kCGEventLeftMouseDragged, kCGMouseButtonLeft};
    }
    if (pressed(kCGMouseButtonRight)) {
        return {kCGEventRightMouseDragged, kCGMouseButtonRight};
    }
    if (pressed(kCGMouseButtonCenter)) {
        return {kCGEventOtherMouseDragged, kCGMouseButtonCenter};
    }
    return {kCGEventMouseMoved, kCGMouseButtonLeft};
}

constexpr CGEventMask bit(CGEventType type) {
    return CGEventMask(1) << type;
}

}  // namespace

void detail::setLastPosted(std::optional<CGPoint> point) {
    std::lock_guard lock(gLastPostedMutex);
    gLastPosted = point;
}

void warp(Point point) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // Deprecated but the only way to cancel the 0.25 s input freeze after a warp.
    CGSetLocalEventsSuppressionInterval(0.0);
#pragma clang diagnostic pop
    CGWarpMouseCursorPosition(CGPointMake(point.x, point.y));
}

EventPoster::EventPoster() : _source(CGEventSourceCreate(kCGEventSourceStateHIDSystemState)) {
    if (_source) {
        // Otherwise each posted event blocks real input for 0.25 s.
        CGEventSourceSetLocalEventsSuppressionInterval(_source, 0.0);
        for (auto state : {kCGEventSuppressionStateSuppressionInterval, kCGEventSuppressionStateRemoteMouseDrag}) {
            CGEventSourceSetLocalEventsFilterDuringSuppressionState(_source, kCGEventFilterMaskPermitAllEvents, state);
        }
    }
}

EventPoster::~EventPoster() {
    detail::setLastPosted(std::nullopt);
    if (_source) {
        CFRelease(_source);
    }
}

void EventPoster::post(Point at) {
    const CGPoint point = CGPointMake(at.x, at.y);
    const auto [type, button] = motionEventKind();
    CGEventRef event = CGEventCreateMouseEvent(_source, type, point, button);
    if (!event) {
        return;
    }
    if (_last) {
        CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, static_cast<int64_t>(std::round(point.x - _last->x)));
        CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, static_cast<int64_t>(std::round(point.y - _last->y)));
    }
    CGEventSetIntegerValueField(event, kCGEventSourceUserData, kOwnEventUserData);
    detail::setLastPosted(point);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
    _last = point;
}

CGEventRef detail::tapCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void *) {
    switch (type) {
        // macOS disables slow taps and taps during secure input; re-enable.
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
            if (CFMachPortRef port = gTapPort.load(std::memory_order_acquire)) {
                CGEventTapEnable(port, true);
            }
            return event;
        default:
            break;
    }
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == kOwnEventUserData) {
        return event;
    }
    switch (type) {
        case kCGEventMouseMoved:
        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:
        case kCGEventOtherMouseDragged:
            return nullptr;
        default:
            if (const auto point = lastPosted()) {
                CGEventSetLocation(event, *point);
            }
            return event;
    }
}

std::expected<std::unique_ptr<MotionBlocker>, std::string> MotionBlocker::start() {
    std::unique_ptr<MotionBlocker> blocker(new MotionBlocker());
    std::promise<std::string> ready;
    auto readyFuture = ready.get_future();

    blocker->_thread = std::thread([self = blocker.get(), &ready] {
        pthread_setname_np("motion-blocker");
        const CGEventMask mask = bit(kCGEventMouseMoved) | bit(kCGEventLeftMouseDragged) | bit(kCGEventRightMouseDragged) |
                                 bit(kCGEventOtherMouseDragged) | bit(kCGEventLeftMouseDown) | bit(kCGEventLeftMouseUp) |
                                 bit(kCGEventRightMouseDown) | bit(kCGEventRightMouseUp) | bit(kCGEventOtherMouseDown) |
                                 bit(kCGEventOtherMouseUp);
        // HID level sees motion before the window server moves the cursor; session level is the fallback.
        CFMachPortRef port = nullptr;
        for (auto location : {kCGHIDEventTap, kCGSessionEventTap}) {
            port = CGEventTapCreate(
              location, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask, detail::tapCallback, nullptr);
            if (port) {
                break;
            }
        }
        if (!port) {
            ready.set_value(
              "cannot install the mouse event tap: allow this app in System Settings → Privacy & Security → "
              "Accessibility, then try again");
            return;
        }
        gTapPort.store(port, std::memory_order_release);
        CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(nullptr, port, 0);
        CFRunLoopRef runLoop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(runLoop, source, kCFRunLoopCommonModes);
        CGEventTapEnable(port, true);
        self->_runLoop.store(runLoop, std::memory_order_release);
        ready.set_value({});

        // Timed runs so a stop issued before the loop started is not missed.
        while (!self->_stop.load(std::memory_order_acquire)) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        }

        CGEventTapEnable(port, false);
        gTapPort.store(nullptr, std::memory_order_release);
        CFMachPortInvalidate(port);
        CFRelease(source);
        CFRelease(port);
    });

    if (auto error = readyFuture.get(); !error.empty()) {
        blocker->_thread.join();
        return std::unexpected(std::move(error));
    }
    return blocker;
}

MotionBlocker::~MotionBlocker() {
    _stop.store(true, std::memory_order_release);
    if (CFRunLoopRef runLoop = _runLoop.load(std::memory_order_acquire)) {
        CFRunLoopStop(runLoop);
    }
    if (_thread.joinable()) {
        _thread.join();
    }
}

}  // namespace macosu_pad
