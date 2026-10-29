#include <ui/core/overlay.h>
#include <ui/core/router.h>

using namespace ui;

Overlay::~Overlay() {
    if (_router) {
        _router->unstashFocus(&_restoreFocus);
    }
}

void Overlay::beginShow(Router *router, Widget *scope, float openDuration, float closeDuration, Easing easing) {
    _router = router;
    _scope = scope;
    _closeDuration = closeDuration;
    _closeEasing = easing;

    _restoreFocus = router->focused();
    router->stashFocus(&_restoreFocus);
    router->pushFocusScope(scope);
    router->setFocus(nullptr);

    _anim.jump(0.0f);
    _anim.to(1.0f, openDuration, easing);
}

void Overlay::update(float dt) {
    _anim.update(dt);

    layoutWrapper(_anim.value());

    if (_closing && _anim.done() && !_removed) {
        _removed = true;
        _router->defer([this] {
            if (!_closing) {
                return;
            }

            onClosed();

            _router->detach(this);
            _router->removeFocusScope(_scope);
            _router->unstashFocus(&_restoreFocus);
            _router->setFocus(_restoreFocus);

            removeFromParent();
        });
    }

    Widget::update(dt);
}

void Overlay::draw(Painter &p) {
    p.beginLayer(_anim.value());
    Widget::draw(p);
    p.endLayer();
}

Widget *Overlay::hitTest(glm::ivec2 point) {
    if (_closing) {
        return nullptr;
    }
    return Widget::hitTest(point);
}

bool Overlay::withinScope(glm::ivec2 point) const {
    return _scope && _scope->frame().contains(point);
}

bool Overlay::handleEvent(Event &e) {
    if (e.kind() == EventKind::click) {
        auto &click = static_cast<ClickEvent &>(e);
        if (_pressOutside && !withinScope(click.position())) {
            dismiss();
        }
        return true;
    }

    if (e.kind() == EventKind::keyDown && static_cast<KeyEvent &>(e).key() == Key::escape) {
        dismiss();
        return true;
    }

    return false;
}

bool Overlay::handleEventCapture(Event &e) {
    if (e.kind() == EventKind::mouseDown) {
        auto &down = static_cast<MouseDownEvent &>(e);
        _pressOutside = !withinScope(down.position());
    }

    return false;
}

void Overlay::dismiss() {
    if (_closing) {
        return;
    }

    _closing = true;
    _anim.to(0.0f, _closeDuration, _closeEasing);
}

void Overlay::cancelDismiss(float duration, Easing easing) {
    _closing = false;
    _removed = false;
    _anim.to(1.0f, duration, easing);
}
