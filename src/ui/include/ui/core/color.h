#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ui {

#define UI_STATIC_COLOR(_name, _r, _g, _b)              \
    constexpr static Color _name(uint8_t alpha = 255) { \
        return Color(_r, _g, _b, alpha);                \
    }

struct Color {
    constexpr Color() : _value(0, 0, 0, 0) {}
    constexpr Color(uint8_t value, uint8_t alpha = 255) : _value(value, value, value, alpha) {}
    constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) : _value(red, green, blue, alpha) {}

    constexpr static Color transparent() {
        return Color(0, 0, 0, 0);
    }

    UI_STATIC_COLOR(white, 255, 255, 255);
    UI_STATIC_COLOR(black, 0, 0, 0);

    constexpr uint8_t r() const {
        return _value.r;
    }

    constexpr uint8_t g() const {
        return _value.g;
    }

    constexpr uint8_t b() const {
        return _value.b;
    }

    constexpr uint8_t a() const {
        return _value.a;
    }

    constexpr Color withAlpha(uint8_t alpha) const {
        return Color(r(), g(), b(), alpha);
    }

private:
    glm::u8vec4 _value;
};

#undef UI_STATIC_COLOR

// Must not outlive the referenced Color, hence rvalue construction is deleted.
struct ColorRef {
    constexpr ColorRef() : _slot(nullptr), _alpha(255) {}
    constexpr ColorRef(const Color &slot) : _slot(&slot), _alpha(255) {}
    constexpr ColorRef(const Color &slot, uint8_t alpha) : _slot(&slot), _alpha(alpha) {}

    ColorRef(const Color &&) = delete;
    ColorRef(const Color &&, uint8_t) = delete;

    constexpr bool valid() const {
        return _slot != nullptr;
    }

    constexpr Color get() const {
        if (!_slot) {
            return Color::transparent();
        }

        if (_alpha != 255) {
            return _slot->withAlpha(_alpha);
        }

        return *_slot;
    }

private:
    const Color *_slot;
    uint8_t _alpha;
};

}  // namespace ui
