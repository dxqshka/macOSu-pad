#include <memory>

#include <ui/core/root.h>

using namespace ui;

namespace {

Widget *addLayer(Widget *parent) {
    auto *layer = parent->add(std::make_unique<Widget>());
    layer->setStyle(Style::fill());
    layer->setPickable(false);
    return layer;
}

}  // namespace

Root::Root() {
    setPickable(false);

    // Order is structural: later children paint and hit-test on top.
    _content = addLayer(this);
    _transient = addLayer(this);
}

void Root::resize(glm::ivec2 size) {
    style().setWidth(Value::pt(static_cast<float>(size.x)));
    style().setHeight(Value::pt(static_cast<float>(size.y)));
}
