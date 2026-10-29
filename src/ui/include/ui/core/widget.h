#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <yoga/Yoga.h>

#include <ui/core/event.h>
#include <ui/core/paint_style.h>
#include <ui/core/painter.h>
#include <ui/core/rect.h>
#include <ui/core/style.h>

namespace ui {

struct Widget;

using WidgetPtr = std::unique_ptr<Widget>;

struct Widget {
    Widget();

    virtual ~Widget();

    virtual void update(float dt);
    virtual void arrange(glm::ivec2 origin);
    virtual void draw(Painter &);

    virtual bool handleEvent(Event &);

    virtual bool handleEventCapture(Event &);

    virtual Widget *hitTest(glm::ivec2 point);

    virtual bool focusable() const;

    virtual bool isFocusScope() const;

    Widget *parent() const;

    int indexInParent() const;

    const Rect &frame() const;

    const std::vector<WidgetPtr> &children() const;

    Widget *add(WidgetPtr child);

    Widget *insert(size_t index, WidgetPtr child);

    WidgetPtr removeAt(size_t index);

    void layout(float availableWidth, float availableHeight);

    bool hovered() const {
        return _hovered;
    }

    bool pressed() const {
        return _pressed;
    }

    bool focused() const {
        return _focused;
    }

    bool focusVisible() const {
        return _focusVisible;
    }

    bool pickable() const {
        return _pickable;
    }

    void setPickable(bool value) {
        _pickable = value;
    }

    void setOverlay(bool value) {
        _overlay = value;
    }

    void setHovered(bool value) {
        _hovered = value;
    }

    void setPressed(bool value) {
        _pressed = value;
    }

    void setFocused(bool value) {
        _focused = value;
    }

    void setFocusVisible(bool value) {
        _focusVisible = value;
    }

    void setStyle(Style style) {
        _style = std::move(style);
    }

    void setPaintStyle(PaintStyle paint) {
        _paint = std::move(paint);
    }

    const PaintStyle &decoration() const {
        return _paint;
    }

    YGNodeRef node() const {
        return _node;
    }

protected:
    virtual void paint(Painter &, const Rect &) {}

    virtual glm::ivec2 measureContent(float maxWidth) const {
        (void)maxWidth;
        return {0, 0};
    }

    // Leaf widgets only: Yoga forbids a measure function on a node with children.
    void enableContentMeasure();

    void markContentDirty();

    Rect frameFromLayout(glm::ivec2 origin) const;

    void setFrame(const Rect &frame) {
        _frame = frame;
    }

    void setParent(Widget *parent) {
        _parent = parent;
    }

    virtual void paintForeground(Painter &, const Rect &) {}

    Style &style() {
        return _style;
    }

    PaintStyle &paintStyle() {
        return _paint;
    }

    WidgetPtr removeFromParent();

private:
    static YGSize yogaMeasureThunk(YGNodeConstRef node, float, YGMeasureMode, float, YGMeasureMode);

    std::vector<WidgetPtr> _children;

    Widget *_parent = nullptr;

    bool _pickable = true;
    bool _overlay = false;
    bool _hovered = false;
    bool _pressed = false;
    bool _focused = false;
    bool _focusVisible = false;

    YGNodeRef _node;
    Rect _frame;
    Style _style;
    PaintStyle _paint;
};

}  // namespace ui
