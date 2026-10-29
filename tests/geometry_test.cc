#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>

#include "overlay_shape.h"
#include "settings/area_geometry.h"
#include "tracker.h"

using namespace macosu_pad;
using namespace settings;

TEST(AreaGeometry, RoundsHalfUpLikeTheOriginalUi) {
    EXPECT_EQ(formatRounded(2.5, 0), "3");
    EXPECT_EQ(formatRounded(-2.5, 0), "-2");
    EXPECT_EQ(formatRounded(-0.001, 2), "0");
    EXPECT_EQ(formatRounded(1512.0, 2), "1512");
    EXPECT_EQ(formatRounded(75.13, 2), "75.13");
}

TEST(AreaGeometry, NormalizesRotationToHalfOpenRange) {
    EXPECT_EQ(normalizeDegrees(190.0), -170.0);
    EXPECT_EQ(normalizeDegrees(180.0), 180.0);
    EXPECT_EQ(normalizeDegrees(-180.0), 180.0);
    EXPECT_EQ(normalizeDegrees(-190.0), 170.0);
}

TEST(AreaGeometry, KeepsAreasOnTheirSurface) {
    EXPECT_EQ(keepCentre(10.0, 50.0, 100.0, 0), 25.0);
    EXPECT_EQ(keepCentre(99.0, 50.0, 100.0, 0), 75.0);
    EXPECT_EQ(keepCentre(10.0, 120.0, 100.0, 0), 40.0);
    EXPECT_EQ(keepCentre(0.0, 33.333, 100.0, 2), 16.67);

    TrackpadArea area{100.0, 20.0, 0.0, 0.0, 90.0};
    fitTrackpad(area, {160.0, 100.0});
    EXPECT_NEAR(area.x, 10.0, 1e-9);
    EXPECT_NEAR(area.y, 50.0, 1e-9);
}

TEST(AreaGeometry, LockAspectMatchesTheDisplayAndFitsTheTrackpad) {
    Config config{
      .trackpadArea = TrackpadArea{150.0, 90.0, 80.0, 50.0, 0.0},
      .displayArea = DisplayArea{1600.0, 900.0, 800.0, 450.0},
      .lockAspectRatio = true,
    };
    lockAspect(config, {160.0, 100.0}, false);
    EXPECT_EQ(config.trackpadArea->width, 150.0);
    EXPECT_EQ(config.trackpadArea->height, 84.38);

    config.trackpadArea->height = 100.0;
    lockAspect(config, {160.0, 100.0}, true);
    EXPECT_EQ(config.trackpadArea->width, 160.0);
    EXPECT_EQ(config.trackpadArea->height, 90.0);

    config.trackpadArea->rotation = 90.0;
    config.trackpadArea->width = 160.0;
    lockAspect(config, {160.0, 100.0}, false);
    EXPECT_EQ(config.trackpadArea->width, 100.0);
    EXPECT_EQ(config.trackpadArea->height, 56.25);

    config.lockAspectRatio = false;
    config.trackpadArea->width = 10.0;
    lockAspect(config, {160.0, 100.0}, false);
    EXPECT_EQ(config.trackpadArea->width, 10.0);
}

TEST(OverlayShape, EllipseSurroundsTheContactCentreWhereTheTrackerMapsIt) {
    const macosu_pad::Size surface{100.0, 50.0};
    const macosu_pad::Size screen{1000.0, 500.0};
    for (const auto direction : {TipDirection::up, TipDirection::finger}) {
        const Config anchored{
          .trackpadArea = TrackpadArea{60.0, 30.0, 50.0, 25.0, 30.0},
          .displayArea = DisplayArea{900.0, 450.0, 500.0, 250.0},
          .tipAnchor = {1.0, 0.5},
          .tipDirection = direction,
        };
        Config plain = anchored;
        plain.tipAnchor = {0.0, 0.0};
        const Touch touch{.path = 1, .x = 0.52, .y = 0.47, .majorAxis = 9.0, .minorAxis = 6.0, .angle = 1.2};
        Touch grown = touch;
        grown.majorAxis = 11.0;
        grown.minorAxis = 7.5;
        std::optional<Tracker::AimSize> aimSize;
        const auto mapped = [&](const Config &config) {
            Tracker tracker(config, screen, surface);
            tracker.onFrame(std::span(&touch, 1), 0.0);
            tracker.onFrame(std::span(&touch, 1), 0.01);
            const auto first = tracker.onFrame(std::span(&touch, 1), 0.03);
            // Unchanged positions are not re-sent, so keep the last one.
            const auto point = tracker.onFrame(std::span(&grown, 1), 0.2);
            aimSize = tracker.aimSize();
            return point ? point : first;
        };
        const auto centre = mapped(plain);
        const auto cursor = mapped(anchored);
        ASSERT_TRUE(cursor && centre && aimSize);

        const auto shape = overlayShape(
          anchored,
          ContactGeometry{11.0f, 7.5f, 1.2f, static_cast<float>(aimSize->majorAxis), static_cast<float>(aimSize->minorAxis)},
          cursor);
        ASSERT_TRUE(shape.has_value());
        Vec2 mean;
        for (const auto &p : shape->ellipse) {
            mean.x += p.x / static_cast<double>(shape->ellipse.size());
            mean.y += p.y / static_cast<double>(shape->ellipse.size());
        }
        EXPECT_NEAR(mean.x, centre->x, 1e-3);
        EXPECT_NEAR(mean.y, centre->y, 1e-3);
        EXPECT_NEAR(shape->aim.x, cursor->x, 1e-9);
    }
}
