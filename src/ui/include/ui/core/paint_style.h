#pragma once

#include <ui/core/color.h>
#include <ui/core/detail/builder.h>
#include <ui/core/painter.h>
#include <ui/core/rect.h>

namespace ui {

struct PaintStyle {
#define UI_PAINT_FIELD(_type, _field, _setField) UI_FLUENT_FIELD(PaintStyle, _type, _field, _setField, )

    UI_PAINT_FIELD(ColorRef, bgColor, setBgColor);
    UI_PAINT_FIELD(ColorRef, borderColor, setBorderColor);
    UI_PAINT_FIELD(float, cornerRadius, setCornerRadius);
    UI_PAINT_FIELD(float, borderWidth, setBorderWidth);

    UI_PAINT_FIELD(Corners, corners, setCorners);

#undef UI_PAINT_FIELD

    constexpr static PaintStyle empty() {
        return {};
    }

    void drawBackground(Painter &p, const Rect &r) const {
        const auto color = _bgColor.get();
        if (color.a() == 0) {
            return;
        }
        p.fillRect(r, color, _cornerRadius, _corners);
    }

    void drawBorder(Painter &p, const Rect &r) const {
        const auto color = _borderColor.get();
        if (_borderWidth <= 0.0f || color.a() == 0) {
            return;
        }
        p.drawRect(r, color, _cornerRadius, _borderWidth, _corners);
    }

protected:
    ColorRef _bgColor;
    ColorRef _borderColor;
    float _cornerRadius = 0.0f;
    float _borderWidth = 0.0f;
    Corners _corners = Corners::all;
};

}  // namespace ui
