#include "config.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <system_error>

#include <toml.hpp>

namespace macosu_pad {

namespace {

bool positive(double v) {
    return std::isfinite(v) && v > 0.0;
}

// Shortest round-trip text of `v` that TOML reads back as a float ("1.0", not "1").
std::string number(double v) {
    auto text = std::format("{}", v);
    if (text.find_first_of(".eEn") == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::string_view boolean(bool v) {
    return v ? "true" : "false";
}

std::unexpected<Error> fail(std::string message) {
    return std::unexpected(std::move(message));
}

using Table = toml::value::table_type;

const toml::value *find(const Table &table, std::string_view key) {
    const auto it = table.find(std::string(key));
    return it == table.end() ? nullptr : &it->second;
}

std::expected<void, Error>
denyUnknown(const Table &table, std::string_view where, std::initializer_list<std::string_view> allowed) {
    for (const auto &[key, _] : table) {
        bool known = false;
        for (auto name : allowed) {
            known = known || key == name;
        }
        if (!known) {
            return fail(
              where.empty() ? std::format("unknown key `{}`", key) : std::format("unknown key `{}` in [{}]", key, where));
        }
    }
    return {};
}

std::expected<void, Error> readBool(const Table &table, std::string_view key, bool &out) {
    const auto *node = find(table, key);
    if (!node) {
        return {};
    }
    if (node->is_boolean()) {
        out = node->as_boolean();
        return {};
    }
    return fail(std::format("`{}` must be true or false", key));
}

std::expected<double, Error> numberValue(const toml::value &node, std::string_view name) {
    if (node.is_floating()) {
        return node.as_floating();
    }
    if (node.is_integer()) {
        return static_cast<double>(node.as_integer());
    }
    return fail(std::format("`{}` must be a number", name));
}

std::expected<void, Error>
readNumber(const Table &table, std::string_view section, std::string_view key, double &out, bool required) {
    const auto *node = find(table, key);
    if (!node) {
        if (required) {
            return fail(std::format("[{}] is missing `{}`", section, key));
        }
        return {};
    }
    auto value = numberValue(*node, std::format("{}.{}", section, key));
    if (!value) {
        return std::unexpected(value.error());
    }
    out = *value;
    return {};
}

const Table *section(const Table &root, std::string_view key, std::expected<void, Error> &status) {
    const auto *node = find(root, key);
    if (!node) {
        return nullptr;
    }
    if (node->is_table()) {
        return &node->as_table();
    }
    status = fail(std::format("`{}` must be a table", key));
    return nullptr;
}

#define RETURN_IF_ERROR(expr)                               \
    if (auto _status = (expr); !_status) {                  \
        return std::unexpected(std::move(_status).error()); \
    }

std::expected<Config, Error> fromToml(const Table &root) {
    RETURN_IF_ERROR(denyUnknown(
      root, "",
      {"trackpad_area", "display_area", "lock_aspect_ratio", "tip_anchor", "tip_direction", "emit_mouse_events",
       "hide_cursor_in_fullscreen", "multitouch_moves_cursor", "theme"}));
    Config config;
    RETURN_IF_ERROR(readBool(root, "lock_aspect_ratio", config.lockAspectRatio));
    RETURN_IF_ERROR(readBool(root, "emit_mouse_events", config.emitMouseEvents));
    RETURN_IF_ERROR(readBool(root, "hide_cursor_in_fullscreen", config.hideCursorInFullscreen));
    RETURN_IF_ERROR(readBool(root, "multitouch_moves_cursor", config.multitouchMovesCursor));

    if (const auto *node = find(root, "tip_anchor")) {
        if (!node->is_array() || node->as_array().size() != 2) {
            return fail("`tip_anchor` must be an array of two numbers [front, width]");
        }
        for (size_t i = 0; i < 2; i++) {
            auto value = numberValue(node->as_array()[i], "tip_anchor");
            if (!value) {
                return std::unexpected(value.error());
            }
            config.tipAnchor[i] = *value;
        }
    }

    if (const auto *node = find(root, "tip_direction")) {
        if (node->is_string() && node->as_string() == "up") {
            config.tipDirection = TipDirection::up;
        } else if (node->is_string() && node->as_string() == "finger") {
            config.tipDirection = TipDirection::finger;
        } else {
            return fail("`tip_direction` must be \"up\" or \"finger\"");
        }
    }

    if (const auto *node = find(root, "theme")) {
        if (node->is_string() && node->as_string() == "dark") {
            config.theme = Theme::dark;
        } else if (node->is_string() && node->as_string() == "light") {
            config.theme = Theme::light;
        } else {
            return fail("`theme` must be \"dark\" or \"light\"");
        }
    }

    std::expected<void, Error> status;
    if (const auto *table = section(root, "trackpad_area", status)) {
        RETURN_IF_ERROR(denyUnknown(*table, "trackpad_area", {"width", "height", "x", "y", "rotation"}));
        TrackpadArea area;
        RETURN_IF_ERROR(readNumber(*table, "trackpad_area", "width", area.width, true));
        RETURN_IF_ERROR(readNumber(*table, "trackpad_area", "height", area.height, true));
        RETURN_IF_ERROR(readNumber(*table, "trackpad_area", "x", area.x, true));
        RETURN_IF_ERROR(readNumber(*table, "trackpad_area", "y", area.y, true));
        RETURN_IF_ERROR(readNumber(*table, "trackpad_area", "rotation", area.rotation, false));
        config.trackpadArea = area;
    }
    RETURN_IF_ERROR(status);

    if (const auto *table = section(root, "display_area", status)) {
        RETURN_IF_ERROR(denyUnknown(*table, "display_area", {"width", "height", "x", "y"}));
        DisplayArea area;
        RETURN_IF_ERROR(readNumber(*table, "display_area", "width", area.width, true));
        RETURN_IF_ERROR(readNumber(*table, "display_area", "height", area.height, true));
        RETURN_IF_ERROR(readNumber(*table, "display_area", "x", area.x, true));
        RETURN_IF_ERROR(readNumber(*table, "display_area", "y", area.y, true));
        config.displayArea = area;
    }
    RETURN_IF_ERROR(status);

    return config;
}

}  // namespace

std::string_view toString(TipDirection direction) {
    switch (direction) {
        case TipDirection::up:
            return "up";
        case TipDirection::finger:
            return "finger";
    }
    return "up";
}

std::string_view toString(Theme theme) {
    switch (theme) {
        case Theme::dark:
            return "dark";
        case Theme::light:
            return "light";
    }
    return "dark";
}

std::expected<void, Error> validate(const Config &config) {
    if (
      const auto &a = config.trackpadArea; a && !(positive(a->width) && positive(a->height) && std::isfinite(a->x) &&
                                                  std::isfinite(a->y) && std::isfinite(a->rotation))) {
        return fail("trackpad_area needs a positive width and height and finite x, y, rotation");
    }
    if (
      const auto &a = config.displayArea;
      a && !(positive(a->width) && positive(a->height) && std::isfinite(a->x) && std::isfinite(a->y))) {
        return fail("display_area needs a positive width and height and finite x, y");
    }
    for (double k : config.tipAnchor) {
        if (!(k >= -kMaxTipAnchor && k <= kMaxTipAnchor)) {
            return fail(
              std::format(
                "tip_anchor values must be in [-{}, {}], got [{}, {}]", kMaxTipAnchor, kMaxTipAnchor,
                number(config.tipAnchor[0]), number(config.tipAnchor[1])));
        }
    }
    return {};
}

std::string render(const Config &config) {
    std::string text = std::format(
      R"(# macOSu-pad settings.

# Post mouse-moved events instead of warping the cursor. Needed by games such
# as osu! (raw input off); blocks the trackpad's own pointer motion while
# tracking. Requires Accessibility permission.
emit_mouse_events = {}

# Hide the macOS cursor while tracking and a fullscreen or borderless app is
# focused (games draw their own). Requires Accessibility permission.
hide_cursor_in_fullscreen = {}

# Keep moving the cursor while more than one finger is on the trackpad.
multitouch_moves_cursor = {}

# Keep the trackpad area's aspect ratio equal to the display area's while
# editing in the settings window.
lock_aspect_ratio = {}

# Fingertip anchoring [front, width]. The trackpad reports the centre of the
# finger's contact ellipse, but you aim with the fingertip. The aim point is the
# centre moved toward the fingertip by front * r - width * minor / 2, with the
# ellipse size averaged over about a second so pressure changes do not move it:
# [0, 0] = contact centre, [1, 0] = front edge, [1, 1] = tip circle.
tip_anchor = [{}, {}]

# "up": toward the top of the trackpad by the mean radius r = sqrt(major *
# minor) / 2, ignoring the ellipse's orientation (steadiest). "finger": along
# the ellipse's major axis, r = major / 2 (its orientation is unreliable on
# nearly round contacts).
tip_direction = "{}"

# Settings window theme: "dark" or "light".
theme = "{}"
)",
      boolean(config.emitMouseEvents), boolean(config.hideCursorInFullscreen), boolean(config.multitouchMovesCursor),
      boolean(config.lockAspectRatio), number(config.tipAnchor[0]), number(config.tipAnchor[1]),
      toString(config.tipDirection), toString(config.theme));

    text +=
      "\n# Active part of the trackpad in millimetres from its top-left corner: size,\n"
      "# centre (x, y) and clockwise rotation in degrees. Omitted = whole trackpad.\n";
    if (const auto &a = config.trackpadArea) {
        text += std::format(
          "[trackpad_area]\nwidth = {}\nheight = {}\nx = {}\ny = {}\nrotation = {}\n", number(a->width), number(a->height),
          number(a->x), number(a->y), number(a->rotation));
    } else {
        text += "# [trackpad_area]\n";
    }

    text +=
      "\n# Part of the main display the trackpad area maps onto, in pixels (macOS\n"
      "# points) from its top-left corner: size and centre. Omitted = whole display.\n";
    if (const auto &a = config.displayArea) {
        text += std::format(
          "[display_area]\nwidth = {}\nheight = {}\nx = {}\ny = {}\n", number(a->width), number(a->height), number(a->x),
          number(a->y));
    } else {
        text += "# [display_area]\n";
    }

    return text;
}

std::expected<Config, Error> parse(std::string_view text) {
    auto result = toml::try_parse_str(std::string(text));
    if (result.is_err()) {
        const auto &errors = result.unwrap_err();
        auto message = errors.empty() ? std::string("invalid TOML") : toml::format_error(errors.front());
        while (!message.empty() && std::isspace(static_cast<unsigned char>(message.back()))) {
            message.pop_back();
        }
        return fail(std::move(message));
    }
    auto config = fromToml(result.unwrap().as_table());
    if (!config) {
        return config;
    }
    RETURN_IF_ERROR(validate(*config));
    return config;
}

std::filesystem::path defaultPath() {
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "") / "Library/Application Support/macOSu-pad/config.toml";
}

std::expected<Config, Error> load(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::string text;
    if (file) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (file.bad()) {
            return fail(std::format("reading {}: read failed", path.string()));
        }
        text = std::move(buffer).str();
    } else {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) || (ec && ec != std::errc::no_such_file_or_directory)) {
            return fail(std::format("reading {}: {}", path.string(), ec ? ec.message() : "cannot open the file"));
        }
    }
    auto config = parse(text);
    if (!config) {
        return fail(std::format("parsing {}: {}", path.string(), config.error()));
    }
    return config;
}

std::expected<void, Error> save(const std::filesystem::path &path, const Config &config) {
    if (const auto dir = path.parent_path(); !dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            return fail(std::format("creating {}: {}", dir.string(), ec.message()));
        }
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << render(config);
    file.close();
    if (!file) {
        return fail(std::format("writing {}: write failed", path.string()));
    }
    return {};
}

}  // namespace macosu_pad
