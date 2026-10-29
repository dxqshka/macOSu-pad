#include "widgets.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include <raylib.h>

#include <ui/core/painter.h>

namespace settings {

namespace {

constexpr float kButtonRadius = 6.0f;
constexpr int kCheckBox = 14;
constexpr int kCheckGap = 6;
constexpr int kFieldPadX = 6;
constexpr int kFieldPadY = 3;
constexpr int kChevronWidth = 16;
constexpr int kSegmentPadX = 12;
constexpr int kSegmentPadY = 3;
constexpr int kSegmentInset = 2;

ui::Painter &painter() {
    return *ui::measurePainter();
}

int textWidth(const ui::Painter &p, std::string_view text, int font) {
    return p.measureText(text, font).size.x;
}

size_t previousCodepoint(std::string_view text, size_t end) {
    size_t start = end;
    while (start > 0) {
        start--;
        if ((static_cast<unsigned char>(text[start]) & 0xC0) != 0x80) {
            break;
        }
    }
    return start;
}

bool isNumberChar(uint32_t c) {
    return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
}

void focusRing(ui::Painter &p, const ui::Rect &r, float radius) {
    const ui::Rect ring(r.left() - 3, r.top() - 3, r.right() + 3, r.bottom() + 3);
    p.drawRect(ring, theme::colors().accentSoft, radius + 3.0f, 3.0f);
}

struct SelectPopup final : ui::Overlay {
    struct Item final : Control {
        Item(SelectPopup *popup, size_t index, std::string label) : _popup(popup), _index(index), _label(std::move(label)) {
            enableContentMeasure();
        }

        bool handleEvent(ui::Event &e) override {
            if (ui::isLeftClick(e)) {
                _popup->choose(_index);
                return true;
            }
            if (e.kind() == ui::EventKind::mouseEnter) {
                _popup->_highlight = _index;
            }
            return false;
        }

    protected:
        glm::ivec2 measureContent(float) const override {
            return {
              textWidth(painter(), _label, theme::fonts::small) + 2 * 12, painter().lineHeight(theme::fonts::small) + 6};
        }

        void paint(ui::Painter &p, const ui::Rect &r) override {
            const bool lit = _popup->_highlight == _index;
            if (lit) {
                p.fillRect(r, theme::colors().accent, 5.0f);
            }
            const int y = r.center().y - p.lineHeight(theme::fonts::small) / 2;
            p.drawText(_label, {r.left() + 12, y}, lit ? ui::Color::white() : theme::colors().fg, theme::fonts::small);
        }

    private:
        SelectPopup *_popup;
        size_t _index;
        std::string _label;
    };

    static void show(Select *owner) {
        auto &h = host();
        auto *popup = static_cast<SelectPopup *>(h.root->transientLayer()->add(std::make_unique<SelectPopup>(owner)));
        popup->beginShow(h.router, popup->_wrapper, 0.12f, 0.08f);
    }

    explicit SelectPopup(Select *owner) : _owner(owner), _highlight(owner->selected().value_or(0)) {
        setOverlay(true);
        setStyle(ui::Style::fill());
        auto wrapper = std::make_unique<Control>();
        wrapper->setStyle(ui::Style::empty());
        for (size_t i = 0; i < owner->options().size(); i++) {
            auto *item = settings::add<Item>(wrapper.get(), this, i, owner->options()[i].label);
            item->tooltip = owner->options()[i].tooltip;
        }
        _wrapper = add(std::move(wrapper));
    }

    bool handleEvent(ui::Event &e) override {
        if (e.kind() == ui::EventKind::keyDown) {
            const auto key = static_cast<ui::KeyEvent &>(e).key();
            const size_t count = _owner->options().size();
            if (key == ui::Key::down) {
                _highlight = (_highlight + 1) % count;
                return true;
            }
            if (key == ui::Key::up) {
                _highlight = (_highlight + count - 1) % count;
                return true;
            }
            if (key == ui::Key::enter || key == ui::Key::space) {
                choose(_highlight);
                return true;
            }
        }
        return ui::Overlay::handleEvent(e);
    }

    void choose(size_t index) {
        if (closing()) {
            return;
        }
        _owner->pick(index);
        dismiss();
    }

protected:
    void layoutWrapper(float anim) override {
        const ui::Rect anchor = _owner->frame();
        const int height = static_cast<int>(_owner->options().size()) * (painter().lineHeight(theme::fonts::small) + 6) + 8;
        const int below = anchor.bottom() + 3;
        const int windowHeight = host().root->frame().height();
        const int top = below + height <= windowHeight ? below : std::max(0, anchor.top() - 3 - height);
        _wrapper->setStyle(
          ui::Style::empty()
            .position(ui::Position::absolute)
            .left(ui::Value::pt(static_cast<float>(anchor.left())))
            .top(ui::Value::pt(static_cast<float>(top) + slideOffset(anim, 4.0f)))
            .minWidth(ui::Value::pt(static_cast<float>(anchor.width())))
            .padding(ui::Edges(4)));
    }

    void paint(ui::Painter &p, const ui::Rect &) override {
        const ui::Rect r = _wrapper->frame();
        p.fillRect(ui::Rect(r.left() + 1, r.top() + 3, r.right() + 1, r.bottom() + 4), ui::Color(0, 0, 0, 40), 8.0f);
        p.fillRect(r, theme::colors().panel, 8.0f);
        p.drawRect(r, theme::colors().border, 8.0f, 1.0f);
    }

private:
    Select *_owner;
    size_t _highlight;
};

}  // namespace

Host &host() {
    static Host value;
    return value;
}

ui::Color resolve(Tone tone) {
    const auto &c = theme::colors();
    switch (tone) {
        case Tone::fg:
            return c.fg;
        case Tone::muted:
            return c.muted;
        case Tone::warn:
            return c.warn;
    }
    return c.fg;
}

void Control::draw(ui::Painter &p) {
    if (!_disabled) {
        ui::Widget::draw(p);
        return;
    }
    p.beginLayer(theme::kDisabledOpacity);
    ui::Widget::draw(p);
    p.endLayer();
}

Text::Text(std::string text, int font, Tone tone, Mode mode) : _spans{{std::move(text), font, tone}}, _mode(mode) {
    enableContentMeasure();
}

void Text::setText(std::string text) {
    if (_spans.size() == 1 && _spans.front().text == text) {
        return;
    }
    const Span first = _spans.empty() ? Span{} : _spans.front();
    _spans = {{std::move(text), first.font, first.tone}};
    invalidate();
}

void Text::setSpans(std::vector<Span> spans) {
    const bool same = std::ranges::equal(spans, _spans, [](const Span &a, const Span &b) {
        return a.text == b.text && a.font == b.font && a.tone == b.tone;
    });
    if (!same) {
        _spans = std::move(spans);
        invalidate();
    }
}

void Text::invalidate() {
    _layoutWidth.reset();
    _ellipsisWidth.reset();
    markContentDirty();
}

const std::vector<Text::Line> &Text::layoutLines(const ui::Painter &p, float maxWidth) const {
    if (_layoutWidth == maxWidth) {
        return _lines;
    }
    _layoutWidth = maxWidth;
    _lines.clear();
    Line line;
    int lineWidth = 0;
    const int baseHeight = _spans.empty() ? 0 : p.lineHeight(_spans.front().font);
    const auto flush = [&] {
        line.height = std::max(line.height, baseHeight);
        _lines.push_back(std::move(line));
        line = {};
        lineWidth = 0;
    };
    for (size_t s = 0; s < _spans.size(); s++) {
        const auto &span = _spans[s];
        const int height = p.lineHeight(span.font);
        std::string_view rest = span.text;
        while (!rest.empty()) {
            if (rest.front() == '\n') {
                line.height = std::max(line.height, height);
                flush();
                rest.remove_prefix(1);
                continue;
            }
            size_t end = rest.find_first_of(" \n");
            end = end == std::string_view::npos ? rest.size() : end;
            size_t withSpaces = end;
            while (withSpaces < rest.size() && rest[withSpaces] == ' ') {
                withSpaces++;
            }
            if (_mode != Mode::wrap) {
                end = withSpaces = std::min(rest.find('\n'), rest.size());
            }
            const std::string_view word = rest.substr(0, end);
            const std::string_view piece = rest.substr(0, withSpaces);
            const int wordWidth = textWidth(p, word, span.font);
            if (_mode == Mode::wrap && !line.pieces.empty() && static_cast<float>(lineWidth + wordWidth) > maxWidth) {
                flush();
            }
            const int pieceWidth = word.size() == piece.size() ? wordWidth : textWidth(p, piece, span.font);
            line.pieces.push_back({s, std::string(piece), lineWidth, pieceWidth});
            line.width = lineWidth + wordWidth;
            lineWidth += pieceWidth;
            line.height = std::max(line.height, height);
            rest.remove_prefix(withSpaces);
        }
    }
    if (!line.pieces.empty() || _lines.empty()) {
        flush();
    }
    return _lines;
}

glm::ivec2 Text::measureContent(float maxWidth) const {
    glm::ivec2 size{0, 0};
    for (const auto &line : layoutLines(painter(), maxWidth)) {
        size.x = std::max(size.x, line.width);
        size.y += line.height;
    }
    return size;
}

void Text::paint(ui::Painter &p, const ui::Rect &r) {
    const auto &lines = layoutLines(p, static_cast<float>(r.width()));
    const bool cut =
      _mode == Mode::ellipsis && !lines.empty() && lines.front().width > r.width() && !lines.front().pieces.empty();
    if (cut && _ellipsisWidth != r.width()) {
        _ellipsisWidth = r.width();
        const int font = _spans[lines.front().pieces.front().span].font;
        _ellipsized = lines.front().pieces.front().text;
        while (!_ellipsized.empty() && textWidth(p, _ellipsized + "…", font) > r.width()) {
            _ellipsized.resize(previousCodepoint(_ellipsized, _ellipsized.size()));
        }
        _ellipsized += "…";
    }
    int y = r.top();
    for (const auto &line : lines) {
        const int x = r.left();
        for (const auto &piece : line.pieces) {
            const auto &span = _spans[piece.span];
            const int dy = (line.height - p.lineHeight(span.font)) / 2;
            p.drawText(
              cut ? std::string_view(_ellipsized) : std::string_view(piece.text), {x + piece.x, y + dy}, resolve(span.tone),
              span.font);
            if (cut) {
                break;
            }
        }
        y += line.height;
    }
}

Button::Button(std::string label, Kind kind) : _label(std::move(label)), _kind(kind) {
    enableContentMeasure();
}

void Button::setLabel(std::string label) {
    if (label != _label) {
        _label = std::move(label);
        markContentDirty();
    }
}

int Button::font() const {
    return _kind == Kind::small ? theme::fonts::small : theme::fonts::body;
}

bool Button::handleEvent(ui::Event &e) {
    const bool activate =
      ui::isLeftClick(e) || (e.kind() == ui::EventKind::keyDown && (static_cast<ui::KeyEvent &>(e).key() == ui::Key::space ||
                                                                    static_cast<ui::KeyEvent &>(e).key() == ui::Key::enter));
    if (!activate) {
        return false;
    }
    if (!_disabled && onClick) {
        onClick();
    }
    return true;
}

glm::ivec2 Button::measureContent(float) const {
    const int padX = _kind == Kind::small ? 10 : 14;
    const int padY = _kind == Kind::small ? 3 : 5;
    return {textWidth(painter(), _label, font()) + 2 * padX + 2, painter().lineHeight(font()) + 2 * padY + 2};
}

void Button::paint(ui::Painter &p, const ui::Rect &r) {
    const auto &c = theme::colors();
    const bool primary = _kind == Kind::primary;
    p.fillRect(r, primary ? c.accent : c.panel, kButtonRadius);
    p.drawRect(r, primary ? c.accent : c.border, kButtonRadius, 1.0f);
    if (pressed() && !_disabled) {
        p.fillRect(r, ui::Color(0, 0, 0, 0x22), kButtonRadius);
    }
    if (focused() && focusVisible()) {
        focusRing(p, r, kButtonRadius);
    }
    const int width = textWidth(p, _label, font());
    p.drawText(
      _label, {r.center().x - width / 2, r.center().y - p.lineHeight(font()) / 2}, primary ? ui::Color::white() : c.fg,
      font());
}

Check::Check(std::string label) {
    setStyle(ui::Style::empty().padding(ui::Edges(kCheckBox + kCheckGap, 0, 0, 0)));
    settings::add<Text>(this, std::move(label), theme::fonts::small, Tone::fg);
}

void Check::toggle() {
    if (_disabled) {
        return;
    }
    _checked = !_checked;
    if (onToggle) {
        onToggle(_checked);
    }
}

bool Check::handleEvent(ui::Event &e) {
    if (
      ui::isLeftClick(e) || (e.kind() == ui::EventKind::keyDown && static_cast<ui::KeyEvent &>(e).key() == ui::Key::space)) {
        toggle();
        return true;
    }
    return false;
}

void Check::paint(ui::Painter &p, const ui::Rect &r) {
    const auto &c = theme::colors();
    const int top = r.top() + (p.lineHeight(theme::fonts::small) - kCheckBox) / 2;
    const ui::Rect box(r.left(), top, r.left() + kCheckBox, top + kCheckBox);
    if (_checked) {
        p.fillRect(box, c.accent, 3.5f);
        const glm::vec2 o(box.left(), box.top());
        p.drawLine(o + glm::vec2(3.5f, 7.5f), o + glm::vec2(6.0f, 10.0f), ui::Color::white(), 1.8f);
        p.drawLine(o + glm::vec2(6.0f, 10.0f), o + glm::vec2(10.8f, 4.2f), ui::Color::white(), 1.8f);
    } else {
        p.fillRect(box, c.bg, 3.5f);
        p.drawRect(box, c.muted, 3.5f, 1.0f);
    }
    if (focused() && focusVisible()) {
        focusRing(p, box, 3.5f);
    }
}

NumberField::NumberField() {
    enableContentMeasure();
}

void NumberField::setShown(std::string text) {
    _shown = std::move(text);
    if (!_editing) {
        _text = _shown;
    }
}

void NumberField::commit() {
    if (_text != _shown && onCommit) {
        const std::string typed = _text;
        onCommit(typed);
    }
    _text = _shown;
    _caret = _anchor = std::min(_caret, _text.size());
}

size_t NumberField::caretAt(int x) const {
    const int local = x - (frame().left() + 1 + kFieldPadX) + _scroll;
    size_t best = 0;
    int bestDistance = std::abs(local);
    for (size_t i = 1; i <= _text.size(); i++) {
        const int distance =
          std::abs(local - textWidth(painter(), std::string_view(_text).substr(0, i), theme::fonts::small));
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

void NumberField::eraseSelection() {
    const auto [from, to] = std::minmax(_anchor, _caret);
    _text.erase(from, to - from);
    _caret = _anchor = from;
}

void NumberField::insert(std::string_view text) {
    eraseSelection();
    std::string filtered;
    for (char ch : text) {
        if (isNumberChar(static_cast<unsigned char>(ch))) {
            filtered += ch;
        }
    }
    _text.insert(_caret, filtered);
    _caret = _anchor = _caret + filtered.size();
}

bool NumberField::handleEvent(ui::Event &e) {
    if (_disabled) {
        return false;
    }
    switch (e.kind()) {
        case ui::EventKind::focusGained:
            _editing = true;
            _text = _shown;
            _anchor = 0;
            _caret = _text.size();
            return true;
        case ui::EventKind::focusLost:
            commit();
            _editing = false;
            _selecting = false;
            _scroll = 0;
            return true;
        case ui::EventKind::mouseDown:
            _caret = _anchor = caretAt(static_cast<ui::PointerEvent &>(e).position().x);
            _selecting = true;
            return true;
        case ui::EventKind::mouseMove:
            if (_selecting) {
                _caret = caretAt(static_cast<ui::PointerEvent &>(e).position().x);
                return true;
            }
            return false;
        case ui::EventKind::mouseUp:
            _selecting = false;
            return true;
        case ui::EventKind::textInput: {
            const uint32_t c = static_cast<ui::TextInputEvent &>(e).codepoint();
            if (isNumberChar(c)) {
                const char ch = static_cast<char>(c);
                insert(std::string_view(&ch, 1));
            }
            return true;
        }
        case ui::EventKind::keyDown: {
            const auto &key = static_cast<ui::KeyEvent &>(e);
            const bool shift = ui::hasModifier(key.modifiers(), ui::Modifier::shift);
            const bool command = ui::hasModifier(key.modifiers(), ui::Modifier::super);
            const auto move = [&](size_t to) {
                _caret = to;
                if (!shift) {
                    _anchor = to;
                }
            };
            switch (key.key()) {
                case ui::Key::enter:
                    commit();
                    _anchor = 0;
                    _caret = _text.size();
                    return true;
                case ui::Key::escape:
                    _text = _shown;
                    _anchor = 0;
                    _caret = _text.size();
                    return true;
                case ui::Key::left:
                    if (!shift && hasSelection()) {
                        move(std::min(_anchor, _caret));
                    } else {
                        move(command ? 0 : (_caret > 0 ? _caret - 1 : 0));
                    }
                    return true;
                case ui::Key::right:
                    if (!shift && hasSelection()) {
                        move(std::max(_anchor, _caret));
                    } else {
                        move(command ? _text.size() : std::min(_caret + 1, _text.size()));
                    }
                    return true;
                case ui::Key::home:
                    move(0);
                    return true;
                case ui::Key::end:
                    move(_text.size());
                    return true;
                case ui::Key::backspace:
                    if (!hasSelection()) {
                        _anchor = command ? 0 : (_caret > 0 ? _caret - 1 : 0);
                    }
                    eraseSelection();
                    return true;
                case ui::Key::del:
                    if (!hasSelection()) {
                        _anchor = std::min(_caret + 1, _text.size());
                    }
                    eraseSelection();
                    return true;
                case ui::Key::up:
                case ui::Key::down:
                    commit();
                    if (onStep) {
                        onStep(key.key() == ui::Key::up ? 1 : -1);
                    }
                    _text = _shown;
                    _caret = _anchor = _text.size();
                    return true;
                case ui::Key::a:
                    if (command) {
                        _anchor = 0;
                        _caret = _text.size();
                        return true;
                    }
                    return false;
                case ui::Key::c:
                case ui::Key::x:
                    if (command && hasSelection()) {
                        const auto [from, to] = std::minmax(_anchor, _caret);
                        SetClipboardText(_text.substr(from, to - from).c_str());
                        if (key.key() == ui::Key::x) {
                            eraseSelection();
                        }
                        return true;
                    }
                    return false;
                case ui::Key::v:
                    if (command) {
                        if (const char *clip = GetClipboardText()) {
                            insert(clip);
                        }
                        return true;
                    }
                    return false;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

glm::ivec2 NumberField::measureContent(float) const {
    return {0, painter().lineHeight(theme::fonts::small) + 2 * kFieldPadY + 2};
}

void NumberField::paint(ui::Painter &p, const ui::Rect &r) {
    const auto &c = theme::colors();
    const bool active = focused() && !_disabled;
    p.fillRect(r, c.bg, kButtonRadius);
    p.drawRect(r, active ? c.accent : c.border, kButtonRadius, 1.0f);
    if (active) {
        focusRing(p, r, kButtonRadius);
    }
    const int font = theme::fonts::small;
    const ui::Rect inner(r.left() + 1 + kFieldPadX, r.top() + 1, r.right() - 1 - kFieldPadX, r.bottom() - 1);
    const auto widthTo = [&](size_t i) {
        return textWidth(p, std::string_view(_text).substr(0, i), font);
    };
    if (active) {
        const int caretX = widthTo(_caret);
        if (caretX - _scroll > inner.width()) {
            _scroll = caretX - inner.width();
        } else if (caretX < _scroll) {
            _scroll = caretX;
        }
    }
    p.pushClip(ui::Rect(inner.left() - 1, inner.top(), inner.right() + 1, inner.bottom()));
    const int textY = r.center().y - p.lineHeight(font) / 2;
    if (active && hasSelection()) {
        const auto [from, to] = std::minmax(_anchor, _caret);
        p.fillRect(
          ui::Rect(
            inner.left() + widthTo(from) - _scroll, textY, inner.left() + widthTo(to) - _scroll, textY + p.lineHeight(font)),
          c.accentSoft.withAlpha(0x66));
    }
    p.drawText(_text, {inner.left() - _scroll, textY}, c.fg, font);
    if (active && !hasSelection()) {
        const float x = static_cast<float>(inner.left() + widthTo(_caret) - _scroll) + 0.5f;
        p.drawLine({x, static_cast<float>(textY + 1)}, {x, static_cast<float>(textY + p.lineHeight(font) - 1)}, c.fg, 1.0f);
    }
    p.popClip();
}

Select::Select(std::vector<Option> options) : _options(std::move(options)) {
    enableContentMeasure();
}

void Select::pick(size_t index) {
    if (index < _options.size() && onChange) {
        onChange(index);
    }
}

void Select::open() {
    if (!_disabled && !_options.empty()) {
        SelectPopup::show(this);
    }
}

bool Select::handleEvent(ui::Event &e) {
    if (ui::isLeftClick(e)) {
        open();
        return true;
    }
    if (e.kind() == ui::EventKind::keyDown) {
        const auto key = static_cast<ui::KeyEvent &>(e).key();
        if (key == ui::Key::space || key == ui::Key::enter || key == ui::Key::down || key == ui::Key::up) {
            open();
            return true;
        }
    }
    return false;
}

glm::ivec2 Select::measureContent(float) const {
    int width = 0;
    for (const auto &option : _options) {
        width = std::max(width, textWidth(painter(), option.label, theme::fonts::small));
    }
    return {width + 2 * kFieldPadX + kChevronWidth + 2, painter().lineHeight(theme::fonts::small) + 2 * kFieldPadY + 2};
}

void Select::paint(ui::Painter &p, const ui::Rect &r) {
    const auto &c = theme::colors();
    p.fillRect(r, c.bg, kButtonRadius);
    p.drawRect(r, c.border, kButtonRadius, 1.0f);
    if (focused() && focusVisible()) {
        focusRing(p, r, kButtonRadius);
    }
    if (_selected && *_selected < _options.size()) {
        const int font = theme::fonts::small;
        p.pushClip(ui::Rect(r.left() + 1, r.top(), r.right() - kChevronWidth, r.bottom()));
        p.drawText(
          _options[*_selected].label, {r.left() + 1 + kFieldPadX, r.center().y - p.lineHeight(font) / 2}, c.fg, font);
        p.popClip();
    }
    const float cx = static_cast<float>(r.right() - kChevronWidth / 2 - 2);
    const float cy = static_cast<float>(r.center().y);
    p.drawLine({cx - 3.0f, cy - 1.5f}, {cx, cy - 4.5f}, c.muted, 1.4f);
    p.drawLine({cx, cy - 4.5f}, {cx + 3.0f, cy - 1.5f}, c.muted, 1.4f);
    p.drawLine({cx - 3.0f, cy + 1.5f}, {cx, cy + 4.5f}, c.muted, 1.4f);
    p.drawLine({cx, cy + 4.5f}, {cx + 3.0f, cy + 1.5f}, c.muted, 1.4f);
}

SegmentedControl::SegmentedControl(std::vector<std::string> labels) : _labels(std::move(labels)) {
    enableContentMeasure();
}

ui::Rect SegmentedControl::segment(size_t index) const {
    const ui::Rect &r = frame();
    const int n = static_cast<int>(_labels.size());
    const int i = static_cast<int>(index);
    const int width = r.width() - 2 * kSegmentInset;
    return ui::Rect(
      r.left() + kSegmentInset + width * i / n, r.top() + kSegmentInset, r.left() + kSegmentInset + width * (i + 1) / n,
      r.bottom() - kSegmentInset);
}

void SegmentedControl::pick(size_t index) {
    if (_disabled || index >= _labels.size() || index == _selected) {
        return;
    }
    _selected = index;
    if (onChange) {
        onChange(index);
    }
}

bool SegmentedControl::handleEvent(ui::Event &e) {
    if (ui::isLeftClick(e)) {
        const int x = static_cast<ui::PointerEvent &>(e).position().x;
        for (size_t i = 0; i < _labels.size(); i++) {
            if (x < segment(i).right() || i + 1 == _labels.size()) {
                pick(i);
                break;
            }
        }
        return true;
    }
    if (e.kind() != ui::EventKind::keyDown || _labels.empty()) {
        return false;
    }
    const size_t last = _labels.size() - 1;
    switch (static_cast<ui::KeyEvent &>(e).key()) {
        case ui::Key::left:
            pick(_selected > 0 ? _selected - 1 : 0);
            return true;
        case ui::Key::right:
            pick(std::min(_selected + 1, last));
            return true;
        case ui::Key::space:
        case ui::Key::enter:
            pick(_selected < last ? _selected + 1 : 0);
            return true;
        default:
            return false;
    }
}

glm::ivec2 SegmentedControl::measureContent(float) const {
    int width = 0;
    for (const auto &label : _labels) {
        width = std::max(width, textWidth(painter(), label, theme::fonts::small));
    }
    const int n = static_cast<int>(_labels.size());
    return {
      n * (width + 2 * kSegmentPadX) + 2 * kSegmentInset,
      painter().lineHeight(theme::fonts::small) + 2 * kSegmentPadY + 2 * kSegmentInset};
}

void SegmentedControl::paint(ui::Painter &p, const ui::Rect &r) {
    const auto &c = theme::colors();
    p.fillRect(r, c.panel, kButtonRadius);
    p.drawRect(r, c.border, kButtonRadius, 1.0f);
    if (focused() && focusVisible()) {
        focusRing(p, r, kButtonRadius);
    }
    const int font = theme::fonts::small;
    for (size_t i = 0; i < _labels.size(); i++) {
        const ui::Rect s = segment(i);
        const bool selected = i == _selected;
        if (selected) {
            p.fillRect(s, c.accent, kButtonRadius - kSegmentInset);
        }
        const int width = textWidth(p, _labels[i], font);
        p.drawText(
          _labels[i], {s.center().x - width / 2, s.center().y - p.lineHeight(font) / 2},
          selected ? ui::Color::white() : c.muted, font);
    }
}

Card::Card() {
    setPickable(false);
}

void Card::paint(ui::Painter &p, const ui::Rect &r) {
    p.fillRect(r, theme::colors().panel, 12.0f);
    p.drawRect(r, theme::colors().border, 12.0f, 1.0f);
}

Banner::Banner() {
    _text = settings::add<Text>(this, "", theme::fonts::body, Tone::fg);
    setStyle(ui::Style::empty().padding(ui::Edges(12, 8, 12, 8)).display(ui::Display::none));
}

void Banner::show(Kind kind, std::string text) {
    _kind = kind;
    _text->setText(std::move(text));
    setStyle(ui::Style::empty().padding(ui::Edges(12, 8, 12, 8)));
}

void Banner::hide() {
    setStyle(ui::Style::empty().padding(ui::Edges(12, 8, 12, 8)).display(ui::Display::none));
}

void Banner::paint(ui::Painter &p, const ui::Rect &r) {
    const ui::Color tint = _kind == Kind::error ? theme::colors().err : theme::colors().ok;
    p.fillRect(r, tint.withAlpha(38), 8.0f);
    p.drawRect(r, tint, 8.0f, 1.0f);
}

void Group::draw(ui::Painter &p) {
    if (_enabled) {
        Control::draw(p);
        return;
    }
    p.beginLayer(0.5f);
    Control::draw(p);
    p.endLayer();
}

ui::Widget *Group::hitTest(glm::ivec2 point) {
    return _enabled ? Control::hitTest(point) : nullptr;
}

ui::Widget *box(ui::Widget *parent, ui::Style style) {
    auto *widget = parent->add(std::make_unique<ui::Widget>());
    widget->setStyle(style);
    widget->setPickable(false);
    return widget;
}

ui::Widget *spacer(ui::Widget *parent) {
    return box(parent, ui::Style::empty().flexGrow(1.0f));
}

const std::string *tooltipAt(ui::Widget *hovered) {
    for (auto *w = hovered; w; w = w->parent()) {
        if (const auto *control = dynamic_cast<const Control *>(w); control && !control->tooltip.empty()) {
            return &control->tooltip;
        }
    }
    return nullptr;
}

}  // namespace settings
