#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

#include "tracker.h"

using namespace macosu_pad;

namespace {

constexpr double W = 1000.0;
constexpr double H = 500.0;
// 1 mm = 10 screen points on both axes with the full areas.
constexpr Size kSurface{100.0, 50.0};

Touch touch(int32_t path, double x, double y) {
    return Touch{.path = path, .x = x, .y = y};
}

Tracker tracker(const Config &config) {
    return Tracker(config, {W, H}, kSurface);
}

std::optional<Point> frame(Tracker &t, std::initializer_list<Touch> touches, double timestamp) {
    return t.onFrame(std::span(touches.begin(), touches.size()), timestamp);
}

double settle(Tracker &t, Touch finger) {
    EXPECT_EQ(frame(t, {finger}, 0.0), std::nullopt);
    EXPECT_EQ(frame(t, {finger}, 0.01), std::nullopt);
    return 0.03;
}

::testing::AssertionResult close(std::optional<Point> a, double x, double y) {
    if (!a) {
        return ::testing::AssertionFailure() << "no position, expected (" << x << ", " << y << ")";
    }
    if (std::abs(a->x - x) < 1e-6 && std::abs(a->y - y) < 1e-6) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "(" << a->x << ", " << a->y << ") vs (" << x << ", " << y << ")";
}

TrackpadArea area(double width, double height, double x, double y, double rotation) {
    return {width, height, x, y, rotation};
}

}  // namespace

TEST(Tracker, SingleFingerMapsAfterGestureTimeout) {
    auto t = tracker({});
    const double at = settle(t, touch(1, 0.5, 0.5));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.25, 0.75)}, at), 250.0, 375.0));
}

TEST(Tracker, AreasMapCentreAndSizeAndClampToScreen) {
    auto t =
      tracker({.trackpadArea = area(50.0, 25.0, 75.0, 12.5, 0.0), .displayArea = DisplayArea{500.0, 250.0, 250.0, 375.0}});
    double at = settle(t, touch(1, 0.75, 0.25));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.75, 0.25)}, at), 250.0, 375.0));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.5, 0.0)}, at + 0.01), 0.0, 250.0));

    auto clamped = tracker({.displayArea = DisplayArea{1000.0, 500.0, 600.0, 250.0}});
    at = settle(clamped, touch(1, 0.95, 0.5));
    EXPECT_TRUE(close(frame(clamped, {touch(1, 0.95, 0.5)}, at), 999.0, 250.0));
}

TEST(Tracker, RotatedAreaMapsAlongItsOwnAxes) {
    auto t = tracker({.trackpadArea = area(40.0, 20.0, 50.0, 25.0, 90.0)});
    const double at = settle(t, touch(1, 0.5, 0.5));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.5, 0.5)}, at), 500.0, 250.0));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.5, 0.7)}, at + 0.01), 750.0, 250.0));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.55, 0.5)}, at + 0.02), 500.0, 125.0));
}

TEST(Tracker, FingerLandingOutsideTheAreaIsIgnoredAndSlidingOutClamps) {
    auto t = tracker({.trackpadArea = area(50.0, 25.0, 50.0, 25.0, 0.0)});
    const double at = settle(t, touch(1, 0.1, 0.1));
    EXPECT_EQ(frame(t, {touch(1, 0.1, 0.1)}, at), std::nullopt);
    EXPECT_TRUE(close(frame(t, {touch(1, 0.5, 0.5)}, at + 0.01), 500.0, 250.0));
    EXPECT_TRUE(close(frame(t, {touch(1, 0.9, 0.6)}, at + 0.02), 999.0, 350.0));
}

TEST(Tracker, MultiFingerGestureNeverMovesCursorUntilAllGestureFingersLift) {
    auto t = tracker({});
    double at = settle(t, touch(1, 0.5, 0.5));
    EXPECT_TRUE(frame(t, {touch(1, 0.5, 0.5)}, at).has_value());
    frame(t, {}, at + 0.01);

    at += 0.02;
    EXPECT_EQ(frame(t, {touch(3, 0.2, 0.2)}, at), std::nullopt);
    for (int i = 1; i < 10; i++) {
        const double dy = i * 0.05;
        EXPECT_EQ(frame(t, {touch(3, 0.2, 0.2 + dy), touch(4, 0.4, 0.2 + dy)}, at + i * 0.01), std::nullopt);
    }
    EXPECT_EQ(frame(t, {touch(4, 0.4, 0.9)}, at + 0.2), std::nullopt);
    EXPECT_EQ(frame(t, {touch(4, 0.4, 0.9)}, at + 0.3), std::nullopt);

    frame(t, {}, at + 0.31);
    at += 0.4;
    frame(t, {touch(5, 0.1, 0.1)}, at);
    EXPECT_TRUE(close(frame(t, {touch(5, 0.1, 0.1)}, at + 0.03), 100.0, 50.0));
}

TEST(Tracker, GestureBeforeAnyPointingDoesNotMoveTheCursor) {
    auto t = tracker({.multitouchMovesCursor = true});
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(frame(t, {touch(1, 0.2, 0.2), touch(2, 0.4, 0.2)}, i * 0.01), std::nullopt);
    }
}

TEST(Tracker, SecondFingerDuringPointingFollowsOriginalFinger) {
    auto t = tracker({.multitouchMovesCursor = true});
    double at = settle(t, touch(1, 0.5, 0.5));
    frame(t, {touch(1, 0.5, 0.5)}, at);
    EXPECT_TRUE(close(frame(t, {touch(2, 0.9, 0.9), touch(1, 0.6, 0.5)}, at + 0.1), 600.0, 250.0));

    auto disabled = tracker({});
    at = settle(disabled, touch(1, 0.5, 0.5));
    frame(disabled, {touch(1, 0.5, 0.5)}, at);
    EXPECT_EQ(frame(disabled, {touch(2, 0.9, 0.9), touch(1, 0.6, 0.5)}, at + 0.1), std::nullopt);
}

namespace {

Touch contact(double yMm, double major, double minor, double angle) {
    return Touch{.path = 1, .x = 0.5, .y = yMm / kSurface.height, .majorAxis = major, .minorAxis = minor, .angle = angle};
}

// Cursor movement in px from the first output to the last.
std::pair<double, double> movement(const Config &config, Touch first, Touch next, int frames = 1) {
    auto t = tracker(config);
    frame(t, {first}, 0.0);
    frame(t, {first}, 0.01);
    const auto start = frame(t, {first}, 0.03);
    EXPECT_TRUE(start.has_value());
    Point end = *start;
    for (int i = 1; i <= frames; i++) {
        end = frame(t, {next}, 0.03 + 0.008 * i).value_or(end);
    }
    return {end.x - start->x, end.y - start->y};
}

}  // namespace

TEST(Tracker, AimFollowsTheTouchSizeAveragedOverASecond) {
    const Config frontEdge{.tipAnchor = {1.0, 0.0}};
    const Touch small = contact(25.0, 6.0, 6.0, 1.571), big = contact(25.0, 8.0, 8.0, 1.571);
    EXPECT_LT(std::abs(movement(frontEdge, small, big).second), 0.2);
    EXPECT_NEAR(movement(frontEdge, small, big, 625).second, -10.0, 0.1);
    EXPECT_EQ(movement({}, small, big, 625).second, 0.0);
}

TEST(Tracker, StraightUpIgnoresOrientationSnaps) {
    const Config frontEdge{.tipAnchor = {1.0, 0.0}};
    const Touch a = contact(25.0, 8.2, 6.8, 1.571), b = contact(25.0, 8.2, 6.8, 3.1);
    const auto [ux, uy] = movement(frontEdge, a, b);
    EXPECT_LT(std::hypot(ux, uy), 1e-9);
    Config along = frontEdge;
    along.tipDirection = TipDirection::finger;
    const auto [fx, fy] = movement(along, a, b);
    EXPECT_GT(std::hypot(fx, fy), 10.0);
}
