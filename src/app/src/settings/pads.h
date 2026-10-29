#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "config.h"
#include "engine.h"
#include "widgets.h"

namespace settings {

struct AreaPad : Control {
    // Surface units.
    std::function<void(double x, double y)> onDrag;
    std::function<void(bool moved)> onDragEnd;

    void setSurface(macosu_pad::Size surface) {
        _surface = surface;
    }

    bool dragging() const {
        return _drag.has_value();
    }

    bool handleEvent(ui::Event &) override;

protected:
    ui::Rect padRect() const;

    double scale() const;

    // `point` in window coordinates.
    virtual bool areaContains(glm::vec2 point) const = 0;
    virtual glm::dvec2 areaCentre() const = 0;

    void paintSurface(ui::Painter &, float radius);

    macosu_pad::Size _surface{1.0, 1.0};

private:
    struct Drag {
        glm::ivec2 start;
        glm::dvec2 centre;
        double unitsPerPx;
        bool moved = false;
    };

    std::optional<Drag> _drag;
};

struct DisplayPad final : AreaPad {
    DisplayPad();

    void setArea(const macosu_pad::DisplayArea &area) {
        _area = area;
    }

    void setCursor(std::optional<macosu_pad::Point> cursor, bool live);

protected:
    void paint(ui::Painter &, const ui::Rect &) override;
    bool areaContains(glm::vec2 point) const override;
    glm::dvec2 areaCentre() const override;

private:
    ui::Rect areaRect() const;

    macosu_pad::DisplayArea _area;
    std::optional<macosu_pad::Point> _cursor;
    bool _live = false;
};

struct TrackpadPad final : AreaPad {
    TrackpadPad();

    void setArea(const macosu_pad::TrackpadArea &area) {
        _area = area;
    }

    // Normalized, origin top-left.
    void setTouches(std::span<const macosu_pad::MonitorFrame::Finger> touches);

protected:
    void paint(ui::Painter &, const ui::Rect &) override;
    bool areaContains(glm::vec2 point) const override;
    glm::dvec2 areaCentre() const override;

private:
    // Window coordinates, clockwise from top-left.
    std::array<glm::vec2, 4> corners(float inset) const;
    glm::vec2 toLocal(glm::vec2 point) const;

    macosu_pad::TrackpadArea _area;
    std::vector<macosu_pad::MonitorFrame::Finger> _touches;
};

}  // namespace settings
