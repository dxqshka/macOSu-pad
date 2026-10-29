#include <gtest/gtest.h>

#include "config.h"

using namespace macosu_pad;

TEST(Config, RenderedFileRoundTrips) {
    EXPECT_EQ(parse(render(Config{})).value(), Config{});

    const Config custom{
      .trackpadArea = TrackpadArea{75.13, 49.29, 76.25, 37.7, -12.5},
      .displayArea = DisplayArea{1280.0, 720.0, 756.0, 491.0},
      .lockAspectRatio = true,
      .tipAnchor = {1.0, 0.35},
      .tipDirection = TipDirection::finger,
      .emitMouseEvents = true,
      .hideCursorInFullscreen = true,
      .multitouchMovesCursor = true,
      .theme = Theme::light,
    };
    const auto parsed = parse(render(custom));
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(*parsed, custom);
}

TEST(Config, RejectsBadValues) {
    for (const char *bad : {
           "[trackpad_area]\nwidth = 0\nheight = 10\nx = 5\ny = 5",
           "[trackpad_area]\nwidth = 10\nheight = 10\nx = 5",
           "[display_area]\nwidth = -1\nheight = 10\nx = 5\ny = 5",
           "tip_anchor = [3.0, 0.0]",
           "unknown_key = 1",
           "tip_direction = \"down\"",
           "theme = \"blue\"",
           "emit_mouse_events = 1",
           "tip_anchor = [1.0]",
           "x = = 1",
         }) {
        EXPECT_FALSE(parse(bad).has_value()) << "accepted " << bad;
    }
}

TEST(Config, IntegersAreAcceptedAsNumbers) {
    const auto parsed = parse("tip_anchor = [1, 0]\n[display_area]\nwidth = 800\nheight = 600\nx = 400\ny = 300");
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->displayArea, (DisplayArea{800.0, 600.0, 400.0, 300.0}));
    EXPECT_EQ(parsed->tipAnchor, (std::array{1.0, 0.0}));
}
