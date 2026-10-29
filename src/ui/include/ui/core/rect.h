#pragma once

#include <glm/glm.hpp>

namespace ui {

// Half-open [min, max): adjacent rects never both claim a point.
struct Rect {
    using Value = glm::ivec2::value_type;

    constexpr Rect() : _min(0, 0), _max(0, 0) {}
    constexpr Rect(glm::ivec2 min, glm::ivec2 max) : _min(min), _max(max) {}
    constexpr Rect(Value left, Value top, Value right, Value bottom) : _min(left, top), _max(right, bottom) {}

    constexpr glm::ivec2 min() const {
        return _min;
    }

    constexpr glm::ivec2 max() const {
        return _max;
    }

    constexpr glm::ivec2 center() const {
        return _min + size() / 2;
    }

    constexpr glm::ivec2 size() const {
        return _max - _min;
    }

    constexpr Value left() const {
        return _min.x;
    }

    constexpr Value right() const {
        return _max.x;
    }

    constexpr Value top() const {
        return _min.y;
    }

    constexpr Value bottom() const {
        return _max.y;
    }

    constexpr Value width() const {
        return right() - left();
    }

    constexpr Value height() const {
        return bottom() - top();
    }

    constexpr bool empty() const {
        return width() <= 0 || height() <= 0;
    }

    constexpr bool contains(glm::ivec2 p) const {
        return p.x >= left() && p.x < right() && p.y >= top() && p.y < bottom();
    }

    constexpr Rect inset(Value amount) const {
        return Rect(_min + glm::ivec2(amount), _max - glm::ivec2(amount));
    }

    constexpr Rect intersect(const Rect &other) const {
        return Rect(glm::max(_min, other._min), glm::min(_max, other._max));
    }

private:
    glm::ivec2 _min;
    glm::ivec2 _max;
};

}  // namespace ui
