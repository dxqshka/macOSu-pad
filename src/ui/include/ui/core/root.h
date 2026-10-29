#pragma once

#include <glm/glm.hpp>

#include <ui/core/widget.h>

namespace ui {

struct Root : Widget {
    Root();

    void resize(glm::ivec2 size);

    Widget *content() {
        return _content;
    }

    Widget *transientLayer() {
        return _transient;
    }

private:
    Widget *_content;
    Widget *_transient;
};

}  // namespace ui
