#pragma once

#include "OctoGui/Tree.h"

#include "Internal/WidgetMetrics.h"

namespace octogui::detail {

struct SplitGeometry {
    Rect first{};
    Rect divider{};
    Rect second{};
    Rect hit{};
};

inline f32 splitClamp(f32 value, f32 minimum, f32 maximum) noexcept {
    if (maximum < minimum) {
        maximum = minimum;
    }

    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

inline Rect splitContentRect(const Tree& tree, NodeHandle node, Rect rect) noexcept {
    const LayoutStyle* layout = tree.layoutStyle(node);

    if (!layout) {
        return rect;
    }

    rect.x += layout->padding.left;
    rect.y += layout->padding.top;

    rect.w -= layout->padding.horizontal();
    rect.h -= layout->padding.vertical();

    if (rect.w < 0.0f) {
        rect.w = 0.0f;
    }

    if (rect.h < 0.0f) {
        rect.h = 0.0f;
    }

    return rect;
}

inline f32 constrainedSplitRatio(const Tree& tree, NodeHandle node, Rect content, f32 ratio) noexcept {
    const SplitContainerState* state = tree.splitContainerState(node);

    if (!state) {
        return 0.5f;
    }

    ratio = splitClamp(ratio, 0.0f, 1.0f);

    const f32 mainSize = state->orientation == SplitOrientation::Horizontal ? content.w : content.h;
    const f32 available = mainSize - SplitDividerWidth;

    if (available <= 0.0f) {
        return 0.5f;
    }

    f32 firstMin = state->minFirst;
    f32 secondMin = state->minSecond;

    if (firstMin < 0.0f) {
        firstMin = 0.0f;
    }

    if (secondMin < 0.0f) {
        secondMin = 0.0f;
    }

    const LayoutStyle* firstLayout = tree.layoutStyle(state->firstPane);
    const LayoutStyle* secondLayout = tree.layoutStyle(state->secondPane);

    if (firstLayout) {
        const f32 childMin = state->orientation == SplitOrientation::Horizontal ? firstLayout->minSize.x : firstLayout->minSize.y;

        if (childMin > firstMin) {
            firstMin = childMin;
        }
    }

    if (secondLayout) {
        const f32 childMin = state->orientation == SplitOrientation::Horizontal ? secondLayout->minSize.x : secondLayout->minSize.y;

        if (childMin > secondMin) {
            secondMin = childMin;
        }
    }

    const f32 totalMin = firstMin + secondMin;

    if (totalMin > available) {
        if (totalMin <= 0.0f) {
            return 0.5f;
        }

        return firstMin / totalMin;
    }

    const f32 minimumRatio = firstMin / available;
    const f32 maximumRatio = 1.0f - secondMin / available;

    return splitClamp(ratio, minimumRatio, maximumRatio);
}

inline SplitGeometry splitGeometry(const Tree& tree, NodeHandle node, Rect rect) noexcept {
    SplitGeometry geometry{};

    const SplitContainerState* state = tree.splitContainerState(node);

    if (!state) {
        return geometry;
    }

    const bool firstVisible = tree.isValid(state->firstPane) && tree.isVisible(state->firstPane);
    const bool secondVisible = tree.isValid(state->secondPane) && tree.isVisible(state->secondPane);

    const Rect content = splitContentRect(tree, node, rect);

    if (!firstVisible && !secondVisible) {
        return geometry;
    }

    if (!firstVisible) {
        geometry.second = content;
        return geometry;
    }

    if (!secondVisible) {
        geometry.first = content;
        return geometry;
    }

    const f32 ratio = constrainedSplitRatio(tree, node, content, state->ratio);

    if (state->orientation == SplitOrientation::Horizontal) {
        const f32 available = content.w - SplitDividerWidth;

        if (available <= 0.0f) {
            return geometry;
        }

        const f32 firstWidth = available * ratio;
        const f32 secondWidth = available - firstWidth;

        geometry.first = Rect{content.x, content.y, firstWidth, content.h};
        geometry.divider = Rect{content.x + firstWidth, content.y, SplitDividerWidth, content.h};
        geometry.second = Rect{geometry.divider.maxX(), content.y, secondWidth, content.h};

        const f32 hitLeft = geometry.divider.x - SplitDividerHitPadding;
        const f32 hitRight = geometry.divider.maxX() + SplitDividerHitPadding;

        geometry.hit = Rect{
            hitLeft < content.x ? content.x : hitLeft,
            content.y,
            hitRight > content.maxX()
                ? content.maxX() - (hitLeft < content.x ? content.x : hitLeft)
                : hitRight - (hitLeft < content.x ? content.x : hitLeft),
            content.h};
    } else {
        const f32 available = content.h - SplitDividerWidth;

        if (available <= 0.0f) {
            return geometry;
        }

        const f32 firstHeight = available * ratio;
        const f32 secondHeight = available - firstHeight;

        geometry.first = Rect{content.x, content.y, content.w, firstHeight};
        geometry.divider = Rect{content.x, content.y + firstHeight, content.w, SplitDividerWidth};
        geometry.second = Rect{content.x, geometry.divider.maxY(), content.w, secondHeight};

        const f32 hitTop = geometry.divider.y - SplitDividerHitPadding;
        const f32 hitBottom = geometry.divider.maxY() + SplitDividerHitPadding;

        geometry.hit = Rect{
            content.x,
            hitTop < content.y ? content.y : hitTop,
            content.w,
            hitBottom > content.maxY()
                ? content.maxY() - (hitTop < content.y ? content.y : hitTop)
                : hitBottom - (hitTop < content.y ? content.y : hitTop)};
    }

    return geometry;
}

} // namespace octogui::detail