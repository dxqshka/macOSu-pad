#pragma once

#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include <ui/core/event.h>

namespace ui {

struct Widget;

struct Router {
    void attachRoot(Widget *root);
    void reset();
    void refreshHover();

    void pointerMoved(glm::ivec2 pos, Modifier modifiers = Modifier::none);
    void pointerButton(MouseButton button, bool down, glm::ivec2 pos, Modifier modifiers = Modifier::none);
    void wheel(glm::ivec2 pos, glm::vec2 offset, Modifier modifiers = Modifier::none);

    void key(Key key, bool down, Modifier modifiers = Modifier::none, bool repeat = false);
    void text(uint32_t codepoint);

    void setFocus(Widget *widget);

    bool focusNext(bool forward);

    void pushFocusScope(Widget *root);
    void removeFocusScope(Widget *root);

    // A widget can't free itself mid-dispatch, so teardown is queued here.
    void defer(std::function<void()> action);
    void runDeferred();

    // Call before removing `subtree` so no dangling pointer survives.
    void detach(Widget *subtree);

    void stashFocus(Widget **slot);
    void unstashFocus(Widget **slot);

    Widget *hovered() const {
        return _hoverLeaf;
    }

    Widget *captured() const {
        return _captured;
    }

    glm::ivec2 pointerPos() const {
        return _lastPos;
    }

    Widget *focused() const {
        return _focus;
    }

private:
    void updateHover(glm::ivec2 pos, Modifier modifiers);

    Widget *focusScope() const;

    Widget *_root = nullptr;
    Widget *_hoverLeaf = nullptr;
    Widget *_pressLeaf = nullptr;
    Widget *_captured = nullptr;
    Widget *_focus = nullptr;
    Widget *_activeScope = nullptr;

    MouseButton _pressButton = MouseButton::none;
    glm::ivec2 _lastPos{0, 0};

    std::vector<Widget *> _focusScopes;
    std::vector<Widget **> _focusStashes;
    std::vector<std::function<void()>> _deferred;
};

}  // namespace ui
