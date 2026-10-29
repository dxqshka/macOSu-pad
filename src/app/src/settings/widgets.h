#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ui/core/overlay.h>
#include <ui/core/root.h>
#include <ui/core/router.h>
#include <ui/core/widget.h>

#include "theme.h"

namespace settings {

struct Host {
    ui::Root *root = nullptr;
    ui::Router *router = nullptr;
};

Host &host();

struct Control : ui::Widget {
    std::string tooltip;

    void setDisabled(bool disabled) {
        _disabled = disabled;
    }

    bool disabled() const {
        return _disabled;
    }

    void draw(ui::Painter &) override;

protected:
    bool _disabled = false;
};

enum struct Tone {
    fg,
    muted,
    warn,
};

ui::Color resolve(Tone);

struct Span {
    std::string text;
    int font = 0;
    Tone tone = Tone::fg;
};

struct Text : Control {
    enum struct Mode {
        wrap,
        single,
        ellipsis,
    };

    Text(std::string text, int font, Tone tone, Mode mode = Mode::wrap);

    void setText(std::string text);
    void setSpans(std::vector<Span> spans);

protected:
    glm::ivec2 measureContent(float maxWidth) const override;
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    struct Piece {
        size_t span;
        std::string text;
        int x;
        int width;
    };

    struct Line {
        std::vector<Piece> pieces;
        int width = 0;
        int height = 0;
    };

    const std::vector<Line> &layoutLines(const ui::Painter &, float maxWidth) const;
    void invalidate();

    std::vector<Span> _spans;
    Mode _mode;
    mutable std::optional<float> _layoutWidth;
    mutable std::vector<Line> _lines;
    std::optional<int> _ellipsisWidth;
    std::string _ellipsized;
};

struct Button : Control {
    enum struct Kind {
        normal,
        primary,
        small,
    };

    std::function<void()> onClick;

    Button(std::string label, Kind kind);

    void setLabel(std::string label);

    bool focusable() const override {
        return !_disabled;
    }

    bool handleEvent(ui::Event &) override;

protected:
    glm::ivec2 measureContent(float maxWidth) const override;
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    int font() const;

    std::string _label;
    Kind _kind;
};

struct Check : Control {
    std::function<void(bool)> onToggle;

    explicit Check(std::string label);

    void setChecked(bool checked) {
        _checked = checked;
    }

    bool focusable() const override {
        return !_disabled;
    }

    bool handleEvent(ui::Event &) override;

protected:
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    void toggle();

    bool _checked = false;
};

struct NumberField : Control {
    std::function<void(std::string_view)> onCommit;
    std::function<void(int)> onStep;

    NumberField();

    void setShown(std::string text);

    bool focusable() const override {
        return !_disabled;
    }

    bool handleEvent(ui::Event &) override;

protected:
    glm::ivec2 measureContent(float maxWidth) const override;
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    size_t caretAt(int x) const;
    void insert(std::string_view text);
    void eraseSelection();
    bool hasSelection() const {
        return _anchor != _caret;
    }
    void commit();

    std::string _shown;
    std::string _text;
    size_t _caret = 0;
    size_t _anchor = 0;
    bool _editing = false;
    bool _selecting = false;
    int _scroll = 0;
};

struct Select : Control {
    struct Option {
        std::string label;
        std::string tooltip;
    };

    std::function<void(size_t)> onChange;

    explicit Select(std::vector<Option> options);

    void setSelected(std::optional<size_t> index) {
        _selected = index;
    }

    std::optional<size_t> selected() const {
        return _selected;
    }

    const std::vector<Option> &options() const {
        return _options;
    }

    bool focusable() const override {
        return !_disabled;
    }

    bool handleEvent(ui::Event &) override;

    void pick(size_t index);

protected:
    glm::ivec2 measureContent(float maxWidth) const override;
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    void open();

    std::vector<Option> _options;
    std::optional<size_t> _selected;
};

struct SegmentedControl : Control {
    std::function<void(size_t)> onChange;

    explicit SegmentedControl(std::vector<std::string> labels);

    void setSelected(size_t index) {
        _selected = index;
    }

    bool focusable() const override {
        return !_disabled;
    }

    bool handleEvent(ui::Event &) override;

protected:
    glm::ivec2 measureContent(float maxWidth) const override;
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    ui::Rect segment(size_t index) const;
    void pick(size_t index);

    std::vector<std::string> _labels;
    size_t _selected = 0;
};

struct Card : Control {
    Card();

protected:
    void paint(ui::Painter &, const ui::Rect &) override;
};

struct Banner : Control {
    enum struct Kind {
        error,
        notice,
    };

    Banner();

    void show(Kind kind, std::string text);
    void hide();

protected:
    void paint(ui::Painter &, const ui::Rect &) override;

private:
    Text *_text;
    Kind _kind = Kind::error;
};

struct Group : Control {
    void setEnabled(bool enabled) {
        _enabled = enabled;
    }

    void draw(ui::Painter &) override;
    ui::Widget *hitTest(glm::ivec2 point) override;

private:
    bool _enabled = true;
};

ui::Widget *box(ui::Widget *parent, ui::Style style);
ui::Widget *spacer(ui::Widget *parent);

template <typename T, typename... Args>
T *add(ui::Widget *parent, Args &&...args) {
    return static_cast<T *>(parent->add(std::make_unique<T>(std::forward<Args>(args)...)));
}

const std::string *tooltipAt(ui::Widget *hovered);

}  // namespace settings
