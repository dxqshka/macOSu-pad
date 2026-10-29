#include <ui/core/widget.h>

#include <algorithm>
#include <limits>

using namespace ui;

namespace {

Painter *gMeasurePainter = nullptr;

}  // namespace

Painter *ui::measurePainter() {
    return gMeasurePainter;
}

void ui::setMeasurePainter(Painter *painter) {
    gMeasurePainter = painter;
}

Widget::Widget() : _node(YGNodeNew()) {}

Widget::~Widget() {
    YGNodeRemoveAllChildren(_node);
    _children.clear();

    if (auto owner = YGNodeGetOwner(_node)) {
        YGNodeRemoveChild(owner, _node);
    }

    YGNodeFree(_node);
}

bool Widget::focusable() const {
    return false;
}

bool Widget::isFocusScope() const {
    return false;
}

Widget *Widget::parent() const {
    return _parent;
}

int Widget::indexInParent() const {
    if (!_parent) {
        return -1;
    }

    const auto &siblings = _parent->_children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [this](const WidgetPtr &c) {
        return c.get() == this;
    });
    return it != siblings.end() ? static_cast<int>(std::distance(siblings.begin(), it)) : -1;
}

const Rect &Widget::frame() const {
    return _frame;
}

const std::vector<WidgetPtr> &Widget::children() const {
    return _children;
}

Widget *Widget::add(WidgetPtr child) {
    return insert(_children.size(), std::move(child));
}

Widget *Widget::insert(size_t index, WidgetPtr child) {
    index = std::min(index, _children.size());

    child->_parent = this;
    YGNodeInsertChild(_node, child->_node, index);

    auto ptr = child.get();
    _children.insert(_children.begin() + index, std::move(child));
    return ptr;
}

WidgetPtr Widget::removeAt(size_t index) {
    if (index >= _children.size()) {
        return nullptr;
    }

    auto child = std::move(_children[index]);
    _children.erase(_children.begin() + index);

    YGNodeRemoveChild(_node, child->_node);
    child->_parent = nullptr;
    return child;
}

WidgetPtr Widget::removeFromParent() {
    const auto index = indexInParent();
    if (index < 0) {
        return nullptr;
    }

    return _parent->removeAt(static_cast<size_t>(index));
}

void Widget::layout(float availableWidth, float availableHeight) {
    YGNodeCalculateLayout(_node, availableWidth, availableHeight, YGDirectionLTR);
}

YGSize Widget::yogaMeasureThunk(YGNodeConstRef node, float width, YGMeasureMode widthMode, float, YGMeasureMode) {
    const auto *self = static_cast<const Widget *>(YGNodeGetContext(node));
    const float maxWidth = widthMode == YGMeasureModeUndefined ? std::numeric_limits<float>::infinity() : width;
    glm::ivec2 size = self ? self->measureContent(maxWidth) : glm::ivec2{0, 0};
    if (widthMode == YGMeasureModeExactly) {
        size.x = static_cast<int>(width);
    } else if (widthMode == YGMeasureModeAtMost) {
        size.x = std::min(size.x, static_cast<int>(width));
    }
    return YGSize{static_cast<float>(size.x), static_cast<float>(size.y)};
}

void Widget::enableContentMeasure() {
    YGNodeSetContext(_node, this);
    YGNodeSetMeasureFunc(_node, &yogaMeasureThunk);
}

void Widget::markContentDirty() {
    if (YGNodeHasMeasureFunc(_node)) {
        YGNodeMarkDirty(_node);
    }
}

void Widget::update(float dt) {
    _style.apply(_node);

    for (const auto &child : _children) {
        child->update(dt);
    }
}

Rect Widget::frameFromLayout(glm::ivec2 origin) const {
    const auto left = static_cast<float>(origin.x) + YGNodeLayoutGetLeft(_node);
    const auto top = static_cast<float>(origin.y) + YGNodeLayoutGetTop(_node);
    const auto right = left + YGNodeLayoutGetWidth(_node);
    const auto bottom = top + YGNodeLayoutGetHeight(_node);

    return {
      static_cast<int>(glm::round(left)),
      static_cast<int>(glm::round(top)),
      static_cast<int>(glm::round(right)),
      static_cast<int>(glm::round(bottom)),
    };
}

void Widget::arrange(glm::ivec2 origin) {
    _frame = frameFromLayout(origin);

    if (YGNodeStyleGetDisplay(_node) == YGDisplayNone) {
        return;
    }

    for (const auto &child : _children) {
        child->arrange(_frame.min());
    }
}

bool Widget::handleEvent(Event &) {
    return false;
}

bool Widget::handleEventCapture(Event &) {
    return false;
}

Widget *Widget::hitTest(glm::ivec2 point) {
    if (YGNodeStyleGetDisplay(_node) == YGDisplayNone || !_frame.contains(point)) {
        return nullptr;
    }

    for (auto it = _children.rbegin(); it != _children.rend(); ++it) {
        if (auto *hit = (*it)->hitTest(point)) {
            return hit;
        }
    }

    return _pickable ? this : nullptr;
}

void Widget::draw(Painter &p) {
    if (YGNodeStyleGetDisplay(_node) == YGDisplayNone) {
        return;
    }

    _paint.drawBackground(p, _frame);

    paint(p, _frame);

    for (const auto &child : _children) {
        if (!child->_overlay) {
            child->draw(p);
        }
    }

    _paint.drawBorder(p, _frame);

    paintForeground(p, _frame);

    for (const auto &child : _children) {
        if (child->_overlay) {
            child->draw(p);
        }
    }
}
