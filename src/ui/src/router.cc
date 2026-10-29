#include <algorithm>

#include <ui/core/router.h>
#include <ui/core/widget.h>

using namespace ui;

namespace {

bool bubble(Widget *from, Event &e) {
    for (auto it = from; it; it = it->parent()) {
        if (it->handleEvent(e)) {
            return true;
        }
    }

    return false;
}

bool capture(Widget *target, Event &e) {
    std::vector<Widget *> path;

    for (auto it = target; it; it = it->parent()) {
        path.push_back(it);
    }

    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if ((*it)->handleEventCapture(e)) {
            return true;
        }
    }

    return false;
}

void setChain(Widget *leaf, void (Widget::*setter)(bool), bool value) {
    for (auto it = leaf; it; it = it->parent()) {
        (it->*setter)(value);
    }
}

Widget *commonAncestor(Widget *a, Widget *b) {
    size_t depthA = 0;
    size_t depthB = 0;

    for (auto it = a; it; it = it->parent()) {
        depthA++;
    }
    for (auto it = b; it; it = it->parent()) {
        depthB++;
    }

    while (a && depthA > depthB) {
        a = a->parent();
        depthA--;
    }
    while (b && depthB > depthA) {
        b = b->parent();
        depthB--;
    }

    while (a != b) {
        a = a ? a->parent() : nullptr;
        b = b ? b->parent() : nullptr;
    }

    return a;
}

Widget *focusableAt(Widget *w) {
    for (; w; w = w->parent()) {
        if (w->focusable()) {
            return w;
        }
    }

    return nullptr;
}

bool isWithin(Widget *w, Widget *root) {
    for (; w; w = w->parent()) {
        if (w == root) {
            return true;
        }
    }

    return false;
}

Widget *focusScopeAt(Widget *w) {
    for (; w; w = w->parent()) {
        if (w->isFocusScope()) {
            return w;
        }
    }

    return nullptr;
}

void collectFocusables(Widget *w, std::vector<Widget *> &out) {
    if (w->focusable()) {
        out.push_back(w);
    }

    for (const auto &child : w->children()) {
        collectFocusables(child.get(), out);
    }
}

}  // namespace

void Router::attachRoot(Widget *root) {
    if (root == _root) {
        return;
    }

    reset();
    _root = root;
}

void Router::reset() {
    setChain(_hoverLeaf, &Widget::setHovered, false);
    setChain(_pressLeaf, &Widget::setPressed, false);

    if (_focus) {
        _focus->setFocused(false);
        _focus->setFocusVisible(false);
    }

    _root = nullptr;
    _hoverLeaf = nullptr;
    _pressLeaf = nullptr;
    _captured = nullptr;
    _focus = nullptr;
    _activeScope = nullptr;
    _pressButton = MouseButton::none;
    _focusScopes.clear();
    _focusStashes.clear();
    _deferred.clear();
}

void Router::refreshHover() {
    updateHover(_lastPos, Modifier::none);
}

void Router::pointerMoved(glm::ivec2 pos, Modifier modifiers) {
    auto delta = pos - _lastPos;
    _lastPos = pos;

    updateHover(pos, modifiers);

    if (delta != glm::ivec2(0, 0)) {
        MouseMoveEvent move(pos, delta, modifiers);
        bubble(_captured ? _captured : _hoverLeaf, move);
    }
}

void Router::pointerButton(MouseButton button, bool down, glm::ivec2 pos, Modifier modifiers) {
    _lastPos = pos;

    if (button == MouseButton::middle) {
        return;
    }

    const auto primary = button == MouseButton::left;

    auto hitTarget = _root ? _root->hitTest(pos) : nullptr;

    if (down) {
        if (_pressButton != MouseButton::none) {
            return;
        }

        MouseDownEvent e(pos, button, modifiers);

        if (capture(hitTarget, e)) {
            return;
        }

        _pressLeaf = hitTarget;
        _pressButton = button;

        if (primary) {
            _activeScope = focusScopeAt(hitTarget);

            setFocus(focusableAt(hitTarget));

            _captured = hitTarget;
        }

        if (_pressLeaf && primary) {
            setChain(_pressLeaf, &Widget::setPressed, true);
            bubble(_pressLeaf, e);
        }
        return;
    }

    if (button != _pressButton) {
        return;
    }

    if (_captured) {
        MouseUpEvent e(pos, button, modifiers);
        bubble(_captured, e);
    }

    if (auto clickTarget = commonAncestor(_pressLeaf, hitTarget)) {
        ClickEvent e(pos, button, modifiers);
        bubble(clickTarget, e);
    }

    if (primary) {
        setChain(_pressLeaf, &Widget::setPressed, false);
    }
    _pressLeaf = nullptr;
    _captured = nullptr;
    _pressButton = MouseButton::none;

    updateHover(pos, modifiers);
}

void Router::wheel(glm::ivec2 pos, glm::vec2 offset, Modifier modifiers) {
    if (auto hitTarget = _root ? _root->hitTest(pos) : nullptr) {
        ScrollEvent e(pos, offset, modifiers);
        bubble(hitTarget, e);
    }
}

void Router::key(Key key, bool down, Modifier modifiers, bool repeat) {
    if (down && key == Key::tab) {
        focusNext(!hasModifier(modifiers, Modifier::shift));
        return;
    }

    auto *target = _focus ? _focus : (_focusScopes.empty() ? nullptr : _focusScopes.back());
    if (!target) {
        return;
    }

    if (_focus) {
        _focus->setFocusVisible(true);
    }

    if (down) {
        KeyDownEvent e(key, modifiers, repeat);
        bubble(target, e);
    } else {
        KeyUpEvent e(key, modifiers);
        bubble(target, e);
    }
}

void Router::text(uint32_t codepoint) {
    if (!_focus) {
        return;
    }

    TextInputEvent e(codepoint);
    bubble(_focus, e);
}

void Router::setFocus(Widget *widget) {
    if (widget == _focus) {
        return;
    }

    if (_focus) {
        _focus->setFocused(false);
        FocusEvent lost(false);
        _focus->handleEvent(lost);
    }

    _focus = widget;

    if (_focus) {
        _focus->setFocused(true);
        _focus->setFocusVisible(false);

        FocusEvent gained(true);
        _focus->handleEvent(gained);
    }
}

bool Router::focusNext(bool forward) {
    auto scope = focusScope();

    if (!scope) {
        return false;
    }

    std::vector<Widget *> items;
    collectFocusables(scope, items);
    if (items.empty()) {
        return false;
    }

    const auto it = std::ranges::find(items, _focus);

    size_t next;
    if (it == items.end()) {
        next = forward ? 0 : items.size() - 1;
    } else {
        const auto current = static_cast<size_t>(std::distance(items.begin(), it));
        next = forward ? (current + 1) % items.size() : (current + items.size() - 1) % items.size();
    }

    setFocus(items[next]);
    if (_focus) {
        _focus->setFocusVisible(true);
    }
    return true;
}

void Router::pushFocusScope(Widget *root) {
    _focusScopes.push_back(root);
}

void Router::removeFocusScope(Widget *root) {
    for (auto it = _focusScopes.rbegin(); it != _focusScopes.rend(); ++it) {
        if (*it == root) {
            _focusScopes.erase(std::next(it).base());
            return;
        }
    }
}

void Router::defer(std::function<void()> action) {
    _deferred.push_back(std::move(action));
}

void Router::runDeferred() {
    auto actions = std::move(_deferred);
    _deferred.clear();

    for (const auto &action : actions) {
        action();
    }
}

void Router::detach(Widget *subtree) {
    if (isWithin(_hoverLeaf, subtree)) {
        setChain(_hoverLeaf, &Widget::setHovered, false);
        _hoverLeaf = nullptr;
    }
    if (isWithin(_pressLeaf, subtree)) {
        setChain(_pressLeaf, &Widget::setPressed, false);
        _pressLeaf = nullptr;
        _pressButton = MouseButton::none;
    }
    if (isWithin(_captured, subtree)) {
        _captured = nullptr;
    }
    if (isWithin(_focus, subtree)) {
        _focus->setFocused(false);
        _focus->setFocusVisible(false);
        _focus = nullptr;
    }
    if (isWithin(_activeScope, subtree)) {
        _activeScope = nullptr;
    }

    for (Widget **slot : _focusStashes) {
        if (isWithin(*slot, subtree)) {
            *slot = nullptr;
        }
    }
}

void Router::stashFocus(Widget **slot) {
    _focusStashes.push_back(slot);
}

void Router::unstashFocus(Widget **slot) {
    std::erase(_focusStashes, slot);
}

void Router::updateHover(glm::ivec2 pos, Modifier modifiers) {
    auto *hit = _root ? _root->hitTest(pos) : nullptr;

    auto hoverTarget = _captured ? commonAncestor(_captured, hit) : hit;

    if (hoverTarget == _hoverLeaf) {
        return;
    }

    if (_hoverLeaf) {
        MouseLeaveEvent leave(pos, modifiers);
        _hoverLeaf->handleEvent(leave);
    }
    setChain(_hoverLeaf, &Widget::setHovered, false);
    _hoverLeaf = hoverTarget;
    setChain(_hoverLeaf, &Widget::setHovered, true);
    if (_hoverLeaf) {
        MouseEnterEvent enter(pos, modifiers);
        _hoverLeaf->handleEvent(enter);
    }
}

Widget *Router::focusScope() const {
    for (auto it = _focusScopes.rbegin(); it != _focusScopes.rend(); ++it) {
        auto scope = *it;

        if (_focus) {
            if (isWithin(_focus, scope)) {
                return scope;
            }
        } else {
            auto host = focusScopeAt(scope);

            if (!host || host == _activeScope) {
                return scope;
            }
        }
    }

    if (auto scope = focusScopeAt(_focus)) {
        return scope;
    }

    return _activeScope;
}
