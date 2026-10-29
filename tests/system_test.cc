#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "cursor_hider.h"
#include "engine.h"
#include "output.h"

using namespace macosu_pad;

namespace {

mt::Touch contact(int32_t path, uint32_t state, float x, float yFromTop) {
    mt::Touch t;
    std::memset(&t, 0, sizeof t);
    t.pathIndex = path;
    t.state = state;
    t.normalized.position.x = x;
    t.normalized.position.y = 1.0f - yFromTop;
    return t;
}

std::optional<macosu_pad::Point> feed(std::vector<mt::Touch> touches, double timestamp) {
    detail::frameCallback(nullptr, touches.data(), static_cast<int>(touches.size()), timestamp, 0);
    return detail::monitorCursor();
}

CGRect rect(double x, double y, double w, double h) {
    return CGRectMake(x, y, w, h);
}

std::pair<bool, CGPoint> filter(CGEventType type, bool postedByApp) {
    CGEventRef event = CGEventCreateMouseEvent(nullptr, type, CGPointMake(10.0, 20.0), kCGMouseButtonLeft);
    EXPECT_NE(event, nullptr);
    if (postedByApp) {
        CGEventSetIntegerValueField(event, kCGEventSourceUserData, kOwnEventUserData);
    }
    const bool passed = detail::tapCallback(nullptr, type, event, nullptr) != nullptr;
    const CGPoint location = CGEventGetLocation(event);
    CFRelease(event);
    return {passed, location};
}

}  // namespace

TEST(Engine, OnlyContactsOnTheSurfaceDriveTheCursor) {
    detail::installTestTracker(Tracker({}, {1000.0, 500.0}, {100.0, 50.0}));
    const auto touching = contact(1, mt::kTouching, 0.5f, 0.5f);
    feed({touching}, 0.0);
    feed({touching}, 0.01);
    const auto at = feed({touching}, 0.03);
    ASSERT_TRUE(at.has_value());
    EXPECT_NEAR(at->x, 500.0, 1e-3);
    EXPECT_NEAR(at->y, 250.0, 1e-3);

    EXPECT_EQ(feed({contact(1, 5, 0.53f, 0.5f)}, 0.04), at);
    feed({}, 0.05);
    // A finger hovering above the pad is not a touch.
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(feed({contact(2, 2, 0.2f, 0.2f)}, 0.06 + i * 0.01), at);
    }
    const auto tap = contact(3, mt::kTouching, 0.8f, 0.8f);
    feed({tap}, 0.2);
    const auto p = feed({tap}, 0.25);
    detail::installTestTracker(std::nullopt);
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->x, 800.0, 1e-3);
    EXPECT_NEAR(p->y, 400.0, 1e-3);
}

TEST(Output, TapDropsForeignMotionAndPinsClicksToPostedPosition) {
    detail::setLastPosted(CGPointMake(300.0, 400.0));
    for (auto motion :
         {kCGEventMouseMoved, kCGEventLeftMouseDragged, kCGEventRightMouseDragged, kCGEventOtherMouseDragged}) {
        EXPECT_FALSE(filter(motion, false).first) << "foreign motion " << motion << " passed";
        const auto [passed, location] = filter(motion, true);
        EXPECT_TRUE(passed && location.x == 10.0 && location.y == 20.0) << "own motion " << motion << " altered";
    }
    for (auto click : {kCGEventLeftMouseDown, kCGEventLeftMouseUp, kCGEventRightMouseDown}) {
        const auto [passed, location] = filter(click, false);
        EXPECT_TRUE(passed && location.x == 300.0 && location.y == 400.0) << "click " << click << " not pinned";
    }
    detail::setLastPosted(std::nullopt);
}

TEST(CursorHider, OnlyWindowsSpanningTheWholeDisplayCountAsFullscreen) {
    const CGRect display = rect(0.0, 0.0, 1512.0, 982.0);
    EXPECT_TRUE(covers(rect(0.0, 0.0, 1512.0, 982.0), display)) << "borderless";
    EXPECT_TRUE(covers(rect(-0.5, 0.0, 1513.0, 982.5), display)) << "rounding slack";
    EXPECT_FALSE(covers(rect(0.0, 33.0, 1512.0, 949.0), display)) << "maximized below menu bar";
    EXPECT_FALSE(covers(rect(100.0, 100.0, 800.0, 600.0), display)) << "normal window";
    const CGRect second = rect(1512.0, 0.0, 1920.0, 1080.0);
    EXPECT_TRUE(covers(rect(1512.0, 0.0, 1920.0, 1080.0), second));
    EXPECT_FALSE(covers(rect(0.0, 0.0, 1512.0, 982.0), second));
}
