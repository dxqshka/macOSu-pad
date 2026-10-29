#pragma once

// Areas follow OpenTabletDriver: a trackpad area in mm maps onto a display area in px.

#include <array>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace macosu_pad {

// A width/height pair: millimetres for trackpads, points for displays.
struct Size {
    double width = 0.0;
    double height = 0.0;

    bool operator==(const Size &) const = default;
};

// Millimetres; x/y is the centre, from the trackpad's top-left corner.
struct TrackpadArea {
    double width = 0.0;
    double height = 0.0;
    double x = 0.0;
    double y = 0.0;
    // Degrees, clockwise as seen from above.
    double rotation = 0.0;

    static TrackpadArea full(Size size) {
        return {size.width, size.height, size.width / 2.0, size.height / 2.0, 0.0};
    }

    bool operator==(const TrackpadArea &) const = default;
};

// Pixels (macOS points); x/y is the centre, from the display's top-left corner.
struct DisplayArea {
    double width = 0.0;
    double height = 0.0;
    double x = 0.0;
    double y = 0.0;

    static DisplayArea full(Size size) {
        return {size.width, size.height, size.width / 2.0, size.height / 2.0};
    }

    bool operator==(const DisplayArea &) const = default;
};

enum struct TipDirection {
    up,
    finger,
};

std::string_view toString(TipDirection);

enum struct Theme {
    dark,
    light,
};

std::string_view toString(Theme);

struct Config {
    // Empty: the whole trackpad.
    std::optional<TrackpadArea> trackpadArea;
    // Empty: the whole main display.
    std::optional<DisplayArea> displayArea;
    bool lockAspectRatio = false;
    std::array<double, 2> tipAnchor{0.0, 0.0};
    TipDirection tipDirection = TipDirection::up;
    bool emitMouseEvents = false;
    bool hideCursorInFullscreen = false;
    bool multitouchMovesCursor = false;
    Theme theme = Theme::dark;

    bool operator==(const Config &) const = default;
};

inline constexpr double kMaxTipAnchor = 2.0;

using Error = std::string;

std::expected<void, Error> validate(const Config &);

std::string render(const Config &);

std::expected<Config, Error> parse(std::string_view text);

std::filesystem::path defaultPath();

// A missing file means the defaults.
std::expected<Config, Error> load(const std::filesystem::path &path);

std::expected<void, Error> save(const std::filesystem::path &path, const Config &config);

}  // namespace macosu_pad
