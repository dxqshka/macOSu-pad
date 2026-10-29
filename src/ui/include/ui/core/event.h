#pragma once

#include <cstdint>
#include <utility>

#include <glm/glm.hpp>

#include <ui/core/key.h>

namespace ui {

enum struct EventKind {
    mouseMove,
    mouseDown,
    mouseUp,
    click,
    mouseEnter,
    mouseLeave,
    scroll,

    keyDown,
    keyUp,
    textInput,

    focusGained,
    focusLost,
};

enum struct MouseButton {
    none,
    left,
    middle,
    right,
};

enum struct Modifier {
    none = 0,
    control = 1 << 0,
    shift = 1 << 1,
    alt = 1 << 2,
    super = 1 << 3,
};

constexpr Modifier operator|(Modifier lhs, Modifier rhs) {
    return static_cast<Modifier>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

constexpr Modifier &operator|=(Modifier &lhs, Modifier rhs) {
    return lhs = lhs | rhs;
}

constexpr bool hasModifier(Modifier set, Modifier mod) {
    return std::to_underlying(set) & std::to_underlying(mod);
}

struct Event {
    constexpr Event(EventKind kind) : _kind(kind) {}

    constexpr EventKind kind() const {
        return _kind;
    }

private:
    EventKind _kind;
};

struct PointerEvent : Event {
    constexpr PointerEvent(EventKind kind, glm::ivec2 position, Modifier modifiers)
        : Event(kind), _position(position), _modifiers(modifiers) {}

    constexpr glm::ivec2 position() const {
        return _position;
    }

    constexpr Modifier modifiers() const {
        return _modifiers;
    }

private:
    glm::ivec2 _position;
    Modifier _modifiers;
};

struct MouseMoveEvent : PointerEvent {
    constexpr MouseMoveEvent(glm::ivec2 position, glm::ivec2 delta, Modifier modifiers = Modifier::none)
        : PointerEvent(EventKind::mouseMove, position, modifiers), _delta(delta) {}

    constexpr glm::ivec2 delta() const {
        return _delta;
    }

private:
    glm::ivec2 _delta;
};

struct MouseButtonEvent : PointerEvent {
    constexpr MouseButtonEvent(EventKind kind, glm::ivec2 position, MouseButton button, Modifier modifiers = Modifier::none)
        : PointerEvent(kind, position, modifiers), _button(button) {}

    constexpr MouseButton button() const {
        return _button;
    }

private:
    MouseButton _button;
};

struct MouseDownEvent : MouseButtonEvent {
    constexpr MouseDownEvent(glm::ivec2 position, MouseButton button, Modifier modifiers = Modifier::none)
        : MouseButtonEvent(EventKind::mouseDown, position, button, modifiers) {}
};

struct MouseUpEvent : MouseButtonEvent {
    constexpr MouseUpEvent(glm::ivec2 position, MouseButton button, Modifier modifiers = Modifier::none)
        : MouseButtonEvent(EventKind::mouseUp, position, button, modifiers) {}
};

struct ClickEvent : MouseButtonEvent {
    constexpr ClickEvent(glm::ivec2 position, MouseButton button, Modifier modifiers = Modifier::none)
        : MouseButtonEvent(EventKind::click, position, button, modifiers) {}
};

struct MouseEnterEvent : PointerEvent {
    constexpr MouseEnterEvent(glm::ivec2 position, Modifier modifiers = Modifier::none)
        : PointerEvent(EventKind::mouseEnter, position, modifiers) {}
};

struct MouseLeaveEvent : PointerEvent {
    constexpr MouseLeaveEvent(glm::ivec2 position, Modifier modifiers = Modifier::none)
        : PointerEvent(EventKind::mouseLeave, position, modifiers) {}
};

struct ScrollEvent : PointerEvent {
    constexpr ScrollEvent(glm::ivec2 position, glm::vec2 offset, Modifier modifiers = Modifier::none)
        : PointerEvent(EventKind::scroll, position, modifiers), _offset(offset) {}

    constexpr glm::vec2 offset() const {
        return _offset;
    }

private:
    glm::vec2 _offset;
};

struct KeyEvent : Event {
    constexpr KeyEvent(EventKind kind, Key key, Modifier modifiers, bool repeat)
        : Event(kind), _key(key), _modifiers(modifiers), _repeat(repeat) {}

    constexpr Key key() const {
        return _key;
    }

    constexpr Modifier modifiers() const {
        return _modifiers;
    }

    constexpr bool repeat() const {
        return _repeat;
    }

private:
    Key _key;
    Modifier _modifiers;
    bool _repeat;
};

struct KeyDownEvent : KeyEvent {
    constexpr KeyDownEvent(Key key, Modifier modifiers = Modifier::none, bool repeat = false)
        : KeyEvent(EventKind::keyDown, key, modifiers, repeat) {}
};

struct KeyUpEvent : KeyEvent {
    constexpr KeyUpEvent(Key key, Modifier modifiers = Modifier::none) : KeyEvent(EventKind::keyUp, key, modifiers, false) {}
};

struct TextInputEvent : Event {
    constexpr TextInputEvent(uint32_t codepoint) : Event(EventKind::textInput), _codepoint(codepoint) {}

    constexpr uint32_t codepoint() const {
        return _codepoint;
    }

private:
    uint32_t _codepoint;
};

struct FocusEvent : Event {
    constexpr FocusEvent(bool gained) : Event(gained ? EventKind::focusGained : EventKind::focusLost) {}

    constexpr bool gained() const {
        return kind() == EventKind::focusGained;
    }
};

inline bool isLeftClick(const Event &e) {
    return e.kind() == EventKind::click && static_cast<const ClickEvent &>(e).button() == MouseButton::left;
}

}  // namespace ui
