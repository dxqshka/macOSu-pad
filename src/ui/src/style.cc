#include <ui/core/style.h>

#include <utility>

using namespace ui;

namespace {

constexpr YGDirection toYoga(LayoutDir value) {
    switch (value) {
        case LayoutDir::inherit:
            return YGDirectionInherit;
        case LayoutDir::leftToRight:
            return YGDirectionLTR;
        case LayoutDir::rightToLeft:
            return YGDirectionRTL;
    }
    std::unreachable();
}

constexpr YGFlexDirection toYoga(FlexDir value) {
    switch (value) {
        case FlexDir::row:
            return YGFlexDirectionRow;
        case FlexDir::rowReverse:
            return YGFlexDirectionRowReverse;
        case FlexDir::column:
            return YGFlexDirectionColumn;
        case FlexDir::columnReverse:
            return YGFlexDirectionColumnReverse;
    }
    std::unreachable();
}

constexpr YGWrap toYoga(FlexWrap value) {
    switch (value) {
        case FlexWrap::noWrap:
            return YGWrapNoWrap;
        case FlexWrap::wrap:
            return YGWrapWrap;
        case FlexWrap::wrapReverse:
            return YGWrapWrapReverse;
    }
    std::unreachable();
}

constexpr YGAlign toYoga(AlignItems value) {
    switch (value) {
        case AlignItems::start:
            return YGAlignFlexStart;
        case AlignItems::end:
            return YGAlignFlexEnd;
        case AlignItems::stretch:
            return YGAlignStretch;
        case AlignItems::center:
            return YGAlignCenter;
    }
    std::unreachable();
}

constexpr YGAlign toYoga(AlignSelf value) {
    switch (value) {
        case AlignSelf::autoValue:
            return YGAlignAuto;
        case AlignSelf::start:
            return YGAlignFlexStart;
        case AlignSelf::end:
            return YGAlignFlexEnd;
        case AlignSelf::stretch:
            return YGAlignStretch;
        case AlignSelf::center:
            return YGAlignCenter;
    }
    std::unreachable();
}

constexpr YGAlign toYoga(AlignContent value) {
    switch (value) {
        case AlignContent::start:
            return YGAlignFlexStart;
        case AlignContent::end:
            return YGAlignFlexEnd;
        case AlignContent::stretch:
            return YGAlignStretch;
        case AlignContent::center:
            return YGAlignCenter;
        case AlignContent::spaceBetween:
            return YGAlignSpaceBetween;
        case AlignContent::spaceAround:
            return YGAlignSpaceAround;
        case AlignContent::spaceEvenly:
            return YGAlignSpaceEvenly;
    }
    std::unreachable();
}

constexpr YGJustify toYoga(Justify value) {
    switch (value) {
        case Justify::start:
            return YGJustifyFlexStart;
        case Justify::end:
            return YGJustifyFlexEnd;
        case Justify::center:
            return YGJustifyCenter;
        case Justify::spaceBetween:
            return YGJustifySpaceBetween;
        case Justify::spaceAround:
            return YGJustifySpaceAround;
        case Justify::spaceEvenly:
            return YGJustifySpaceEvenly;
    }
    std::unreachable();
}

void applyEdge(YGNodeRef node, YGEdge edge, Value value) {
    if (value.isPoint()) {
        YGNodeStyleSetPosition(node, edge, value.get());
    } else if (value.isPercent()) {
        YGNodeStyleSetPositionPercent(node, edge, value.get());
    } else {
        YGNodeStyleSetPosition(node, edge, YGUndefined);
    }
}

using DimAuto = void (*)(YGNodeRef);
using DimSet = void (*)(YGNodeRef, float);

void applyDimension(YGNodeRef node, Value value, DimAuto setAuto, DimSet setPoint, DimSet setPercent) {
    if (value.isAuto()) {
        if (setAuto) {
            setAuto(node);
        }
    } else if (value.isPoint()) {
        setPoint(node, value.get());
    } else if (value.isPercent()) {
        setPercent(node, value.get());
    } else {
        setPoint(node, YGUndefined);
    }
}

}  // namespace

void Style::apply(YGNodeRef node) {
    if (!_dirty) {
        return;
    }

    YGNodeStyleSetDirection(node, toYoga(_layoutDir));
    YGNodeStyleSetAlignContent(node, toYoga(_alignContent));
    YGNodeStyleSetAlignItems(node, toYoga(_alignItems));
    YGNodeStyleSetJustifyContent(node, toYoga(_justifyContent));
    YGNodeStyleSetFlexDirection(node, toYoga(_flexDir));
    YGNodeStyleSetFlexWrap(node, toYoga(_flexWrap));

    YGNodeStyleSetPositionType(node, _position == Position::absolute ? YGPositionTypeAbsolute : YGPositionTypeRelative);

    applyEdge(node, YGEdgeLeft, _left);
    applyEdge(node, YGEdgeRight, _right);
    applyEdge(node, YGEdgeTop, _top);
    applyEdge(node, YGEdgeBottom, _bottom);

    YGNodeStyleSetMargin(node, YGEdgeLeft, _margin.left());
    YGNodeStyleSetMargin(node, YGEdgeRight, _margin.right());
    YGNodeStyleSetMargin(node, YGEdgeTop, _margin.top());
    YGNodeStyleSetMargin(node, YGEdgeBottom, _margin.bottom());

    YGNodeStyleSetPadding(node, YGEdgeLeft, _padding.left());
    YGNodeStyleSetPadding(node, YGEdgeRight, _padding.right());
    YGNodeStyleSetPadding(node, YGEdgeTop, _padding.top());
    YGNodeStyleSetPadding(node, YGEdgeBottom, _padding.bottom());

    applyDimension(node, _width, YGNodeStyleSetWidthAuto, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent);
    applyDimension(node, _height, YGNodeStyleSetHeightAuto, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent);
    applyDimension(node, _minWidth, nullptr, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent);
    applyDimension(node, _minHeight, nullptr, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent);
    applyDimension(node, _maxWidth, nullptr, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent);
    applyDimension(node, _maxHeight, nullptr, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent);
    applyDimension(node, _flexBasis, YGNodeStyleSetFlexBasisAuto, YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent);

    if (_gap.isPoint()) {
        YGNodeStyleSetGap(node, YGGutterAll, _gap.get());
    } else if (_gap.isPercent()) {
        YGNodeStyleSetGapPercent(node, YGGutterAll, _gap.get());
    } else {
        YGNodeStyleSetGap(node, YGGutterAll, YGUndefined);
    }

    YGNodeStyleSetFlexGrow(node, _flexGrow);
    YGNodeStyleSetFlexShrink(node, _flexShrink);
    YGNodeStyleSetAlignSelf(node, toYoga(_alignSelf));
    YGNodeStyleSetDisplay(node, _display == Display::none ? YGDisplayNone : YGDisplayFlex);

    _dirty = false;
}
