#include <ui/backend/raylib_input.h>

#include <array>
#include <cmath>
#include <utility>

#include <raylib.h>

using namespace ui;

namespace {

constexpr std::array<std::pair<int, Key>, 17> kKeys{{
  {KEY_ENTER, Key::enter},
  {KEY_KP_ENTER, Key::enter},
  {KEY_TAB, Key::tab},
  {KEY_BACKSPACE, Key::backspace},
  {KEY_DELETE, Key::del},
  {KEY_ESCAPE, Key::escape},
  {KEY_SPACE, Key::space},
  {KEY_LEFT, Key::left},
  {KEY_RIGHT, Key::right},
  {KEY_UP, Key::up},
  {KEY_DOWN, Key::down},
  {KEY_HOME, Key::home},
  {KEY_END, Key::end},
  {KEY_A, Key::a},
  {KEY_C, Key::c},
  {KEY_V, Key::v},
  {KEY_X, Key::x},
}};

Modifier modifiers() {
    Modifier m = Modifier::none;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        m |= Modifier::control;
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        m |= Modifier::shift;
    }
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
        m |= Modifier::alt;
    }
    if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) {
        m |= Modifier::super;
    }
    return m;
}

glm::ivec2 pointer() {
    const Vector2 p = GetMousePosition();
    return {static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))};
}

}  // namespace

bool ui::pumpRaylibInput(Router &router) {
    bool any = false;
    const Modifier mods = modifiers();
    const glm::ivec2 pos = pointer();

    if (pos != router.pointerPos()) {
        router.pointerMoved(pos, mods);
        any = true;
    }

    constexpr std::array<std::pair<int, MouseButton>, 3> buttons{{
      {MOUSE_BUTTON_LEFT, MouseButton::left},
      {MOUSE_BUTTON_RIGHT, MouseButton::right},
      {MOUSE_BUTTON_MIDDLE, MouseButton::middle},
    }};
    for (const auto &[raylibButton, button] : buttons) {
        if (IsMouseButtonPressed(raylibButton)) {
            router.pointerButton(button, true, pos, mods);
            any = true;
        }
        if (IsMouseButtonReleased(raylibButton)) {
            router.pointerButton(button, false, pos, mods);
            any = true;
        }
    }

    if (const Vector2 wheel = GetMouseWheelMoveV(); wheel.x != 0.0f || wheel.y != 0.0f) {
        router.wheel(pos, {wheel.x, wheel.y}, mods);
        any = true;
    }

    for (const auto &[raylibKey, key] : kKeys) {
        const bool pressed = IsKeyPressed(raylibKey);
        const bool repeat = !pressed && IsKeyPressedRepeat(raylibKey);
        if (pressed || repeat) {
            router.key(key, true, mods, repeat);
            any = true;
        }
        if (IsKeyReleased(raylibKey)) {
            router.key(key, false, mods);
            any = true;
        }
    }
    // Drain the raw key queue so modifier-only presses also count as activity.
    while (GetKeyPressed() != 0) {
        any = true;
    }

    for (int codepoint = GetCharPressed(); codepoint != 0; codepoint = GetCharPressed()) {
        router.text(static_cast<uint32_t>(codepoint));
        any = true;
    }
    return any;
}
