#pragma once

#include <yoga/Yoga.h>
#include <glm/glm.hpp>

#include <ui/core/detail/builder.h>

namespace ui {

struct Value {
    constexpr static Value undefined() {
        return Value(Kind::undefined, 0.0f);
    }

    constexpr static Value autoValue() {
        return Value(Kind::autoValue, 0.0f);
    }

    constexpr static Value pt(float v) {
        return Value(Kind::point, v);
    }

    constexpr static Value pct(float v) {
        return Value(Kind::percent, v);
    }

    constexpr bool isAuto() const {
        return _kind == Kind::autoValue;
    }

    constexpr bool isPoint() const {
        return _kind == Kind::point;
    }

    constexpr bool isPercent() const {
        return _kind == Kind::percent;
    }

    constexpr float get() const {
        return _value;
    }

private:
    enum struct Kind { undefined, autoValue, point, percent };

    constexpr Value(Kind kind, float value) : _kind(kind), _value(value) {}

    Kind _kind;
    float _value;
};

constexpr Value operator""_px(unsigned long long value) {
    return Value::pt(static_cast<float>(value));
}

constexpr Value operator""_pct(unsigned long long value) {
    return Value::pct(static_cast<float>(value));
}

enum struct LayoutDir {
    inherit,
    leftToRight,
    rightToLeft,
};

enum struct FlexDir {
    row,
    rowReverse,
    column,
    columnReverse,
};

enum struct FlexWrap {
    noWrap,
    wrap,
    wrapReverse,
};

enum struct Position {
    relative,
    absolute,
};

enum struct AlignItems {
    start,
    end,
    stretch,
    center,
};

enum struct AlignContent {
    start,
    end,
    stretch,
    center,
    spaceBetween,
    spaceAround,
    spaceEvenly,
};

enum struct AlignSelf {
    autoValue,
    start,
    end,
    stretch,
    center,
};

enum struct Display {
    flex,
    none,
};

enum struct Justify {
    start,
    end,
    center,
    spaceBetween,
    spaceAround,
    spaceEvenly,
};

struct Edges {
    constexpr Edges() : _topLeft(0, 0), _bottomRight(0, 0) {}
    constexpr Edges(int value) : _topLeft(value, value), _bottomRight(value, value) {}
    constexpr Edges(int left, int top, int right, int bottom) : _topLeft(left, top), _bottomRight(right, bottom) {}
    constexpr Edges(glm::ivec2 topLeft, glm::ivec2 bottomRight) : _topLeft(topLeft), _bottomRight(bottomRight) {}

    constexpr int left() const {
        return _topLeft.x;
    }

    constexpr int right() const {
        return _bottomRight.x;
    }

    constexpr int top() const {
        return _topLeft.y;
    }

    constexpr int bottom() const {
        return _bottomRight.y;
    }

private:
    glm::ivec2 _topLeft;
    glm::ivec2 _bottomRight;
};

struct Style {
    void apply(YGNodeRef);

#define UI_STYLE_FIELD(_type, _field, _setField) UI_FLUENT_FIELD(Style, _type, _field, _setField, _dirty = true)

    UI_STYLE_FIELD(LayoutDir, layoutDir, setLayoutDir);
    UI_STYLE_FIELD(AlignContent, alignContent, setAlignContent);
    UI_STYLE_FIELD(AlignItems, alignItems, setAlignItems);
    UI_STYLE_FIELD(Justify, justifyContent, setJustifyContent);
    UI_STYLE_FIELD(FlexDir, flexDir, setFlexDir);
    UI_STYLE_FIELD(FlexWrap, flexWrap, setFlexWrap);

    UI_STYLE_FIELD(Position, position, setPosition);
    UI_STYLE_FIELD(Value, left, setLeft);
    UI_STYLE_FIELD(Value, top, setTop);
    UI_STYLE_FIELD(Value, right, setRight);
    UI_STYLE_FIELD(Value, bottom, setBottom);

    UI_STYLE_FIELD(Edges, margin, setMargin);
    UI_STYLE_FIELD(Edges, padding, setPadding);

    UI_STYLE_FIELD(Value, width, setWidth);
    UI_STYLE_FIELD(Value, height, setHeight);
    UI_STYLE_FIELD(Value, minWidth, setMinWidth);
    UI_STYLE_FIELD(Value, minHeight, setMinHeight);
    UI_STYLE_FIELD(Value, maxWidth, setMaxWidth);
    UI_STYLE_FIELD(Value, maxHeight, setMaxHeight);
    UI_STYLE_FIELD(Value, flexBasis, setFlexBasis);
    UI_STYLE_FIELD(Value, gap, setGap);

    UI_STYLE_FIELD(float, flexGrow, setFlexGrow);
    UI_STYLE_FIELD(float, flexShrink, setFlexShrink);
    UI_STYLE_FIELD(AlignSelf, alignSelf, setAlignSelf);
    UI_STYLE_FIELD(Display, display, setDisplay);

#undef UI_STYLE_FIELD

    constexpr static Style empty() {
        return {};
    }

    constexpr static Style fill() {
        return empty().position(Position::absolute).left(0_px).top(0_px).right(0_px).bottom(0_px);
    }

    constexpr static Style
    flex(FlexDir dir, float flexGrow = 0.0f, float flexShrink = 1.0f, Value flexBasis = Value::undefined()) {
        return empty().flexDir(dir).flexGrow(flexGrow).flexShrink(flexShrink).flexBasis(flexBasis);
    }

protected:
    bool _dirty = true;

    LayoutDir _layoutDir = LayoutDir::inherit;
    AlignContent _alignContent = AlignContent::start;
    AlignItems _alignItems = AlignItems::stretch;
    Justify _justifyContent = Justify::start;
    FlexDir _flexDir = FlexDir::column;
    FlexWrap _flexWrap = FlexWrap::noWrap;

    Position _position = Position::relative;
    Value _left = Value::undefined();
    Value _top = Value::undefined();
    Value _right = Value::undefined();
    Value _bottom = Value::undefined();

    Edges _margin;
    Edges _padding;

    Value _width = Value::autoValue();
    Value _height = Value::autoValue();
    Value _minWidth = Value::undefined();
    Value _minHeight = Value::undefined();
    Value _maxWidth = Value::undefined();
    Value _maxHeight = Value::undefined();
    Value _flexBasis = Value::undefined();
    Value _gap = Value::undefined();

    float _flexGrow = 0.0f;
    float _flexShrink = 1.0f;
    AlignSelf _alignSelf = AlignSelf::autoValue;
    Display _display = Display::flex;
};

}  // namespace ui
