#pragma once

#include <atomic>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <ApplicationServices/ApplicationServices.h>

#include "tracker.h"

namespace macosu_pad {

// Set in kCGEventSourceUserData of every event this app posts; the event tap lets these through.
inline constexpr int64_t kOwnEventUserData = 0x4154'524B;

void warp(Point point);

struct EventPoster {
    EventPoster();
    ~EventPoster();

    EventPoster(const EventPoster &) = delete;
    EventPoster &operator=(const EventPoster &) = delete;

    void post(Point point);

private:
    CGEventSourceRef _source;
    std::optional<CGPoint> _last;
};

// Event tap that drops trackpad relative motion we did not post and pins clicks to the last posted position.
struct MotionBlocker {
    static std::expected<std::unique_ptr<MotionBlocker>, std::string> start();

    ~MotionBlocker();

    MotionBlocker(const MotionBlocker &) = delete;
    MotionBlocker &operator=(const MotionBlocker &) = delete;

private:
    MotionBlocker() = default;

    std::atomic<CFRunLoopRef> _runLoop = nullptr;
    std::atomic<bool> _stop = false;
    std::thread _thread;
};

namespace detail {

CGEventRef tapCallback(CGEventTapProxy, CGEventType, CGEventRef, void *);

void setLastPosted(std::optional<CGPoint>);

}  // namespace detail

}  // namespace macosu_pad
