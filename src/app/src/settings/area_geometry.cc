#include "area_geometry.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace settings {

namespace {

double radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

}  // namespace

double roundTo(double value, int digits) {
    const double f = std::pow(10.0, digits);
    return std::floor(value * f + 0.5) / f;
}

std::string formatRounded(double value, int digits) {
    const double rounded = roundTo(value, digits);
    return std::format("{}", rounded == 0.0 ? 0.0 : rounded);
}

double normalizeDegrees(double degrees) {
    return degrees - 360.0 * std::ceil((degrees - 180.0) / 360.0);
}

double keepCentre(double centre, double extent, double total, int digits) {
    const double f = std::pow(10.0, digits);
    const double a = extent / 2.0;
    const double b = total - extent / 2.0;
    const double lo = std::min(a, b);
    const double hi = std::max(a, b);
    // Bounds rounded inward, so the rounded centre stays in range.
    const double rlo = std::ceil(lo * f - 1e-6) / f;
    const double rhi = std::floor(hi * f + 1e-6) / f;
    return rlo <= rhi ? std::clamp(roundTo(centre, digits), rlo, rhi) : std::clamp(centre, lo, hi);
}

macosu_pad::Size rotatedExtent(const macosu_pad::TrackpadArea &area) {
    const double c = std::abs(std::cos(radians(area.rotation)));
    const double s = std::abs(std::sin(radians(area.rotation)));
    return {area.width * c + area.height * s, area.width * s + area.height * c};
}

macosu_pad::Size trackpadLimits(const macosu_pad::TrackpadArea &area, macosu_pad::Size trackpad) {
    const double r = radians(area.rotation);
    return std::abs(std::sin(r)) > std::abs(std::cos(r)) ? macosu_pad::Size{trackpad.height, trackpad.width} : trackpad;
}

void fitDisplay(macosu_pad::DisplayArea &area, macosu_pad::Size screen) {
    area.x = keepCentre(area.x, area.width, screen.width, 0);
    area.y = keepCentre(area.y, area.height, screen.height, 0);
}

void fitTrackpad(macosu_pad::TrackpadArea &area, macosu_pad::Size trackpad) {
    const macosu_pad::Size extent = rotatedExtent(area);
    area.x = keepCentre(area.x, extent.width, trackpad.width, 2);
    area.y = keepCentre(area.y, extent.height, trackpad.height, 2);
}

void lockAspect(macosu_pad::Config &config, macosu_pad::Size trackpad, bool keepHeight) {
    if (!config.lockAspectRatio) {
        return;
    }
    auto &t = *config.trackpadArea;
    const auto &d = *config.displayArea;
    const double ratio = d.width / d.height;
    const double w = keepHeight ? t.height * ratio : t.width;
    const double h = keepHeight ? t.height : t.width / ratio;
    const macosu_pad::Size limits = trackpadLimits(t, trackpad);
    const double fit = std::min({1.0, limits.width / w, limits.height / h});
    t.width = roundTo(w * fit, 2);
    t.height = roundTo(h * fit, 2);
}

}  // namespace settings
