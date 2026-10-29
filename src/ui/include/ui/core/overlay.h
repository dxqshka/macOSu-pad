#pragma once

#include <ui/core/anim.h>
#include <ui/core/rect.h>
#include <ui/core/widget.h>

namespace ui {

struct Router;

struct Overlay : Widget {
    ~Overlay() override;

    void update(float dt) override;
    void draw(Painter &) override;
    bool handleEvent(Event &) override;
    bool handleEventCapture(Event &) override;

    Widget *hitTest(glm::ivec2 point) override;

    void dismiss();

protected:
    void beginShow(Router *router, Widget *scope, float openDuration, float closeDuration, Easing easing = easing::cubicOut);

    void cancelDismiss(float duration, Easing easing);

    static float slideOffset(float anim, float slide) {
        return (1.0f - anim) * -slide;
    }

    float anim() const {
        return _anim.value();
    }

    bool closing() const {
        return _closing;
    }

    virtual void layoutWrapper(float anim) = 0;

    virtual bool withinScope(glm::ivec2 point) const;

    virtual void onClosed() {}

    Router *_router = nullptr;
    Widget *_wrapper = nullptr;
    Widget *_scope = nullptr;
    Widget *_restoreFocus = nullptr;

    Tween _anim;
    Easing _closeEasing = easing::cubicOut;
    float _closeDuration = 0.0f;

    bool _pressOutside = false;
    bool _closing = false;
    bool _removed = false;
};

}  // namespace ui
