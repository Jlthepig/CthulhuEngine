#include "OctoGui/Layout.h"
#include "OctoGui/Tree.h"

#include "Internal/SplitLayoutInternal.h"
namespace octogui {
namespace {
const LayoutStyle &styleOrDefault(const Tree &tree, NodeHandle node) {
  static const LayoutStyle fallback{};
  const LayoutStyle *style = tree.layoutStyle(node);
  return style ? *style : fallback;
}

f32 maxZero(f32 value) noexcept { return value > 0.0 ? value : 0.0; }

f32 clampFloat(f32 value, f32 minValue, f32 maxValue) noexcept {
  if (maxValue < minValue) {
    maxValue = minValue;
  }

  if (value < minValue) {
    value = minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

f32 measureNaturalHeight(
    const Tree &tree,
    NodeHandle node);

f32 childDesiredHeight(
    const Tree &tree,
    NodeHandle child);

Vec2 clampSize(Vec2 size, Vec2 minSize, Vec2 maxSize) noexcept {
  f32 x = size.x;
  f32 y = size.y;

  f32 minX = minSize.x;
  f32 maxX = maxSize.x;

  f32 minY = minSize.y;
  f32 maxY = maxSize.y;

  if (maxX < minX) {
    maxX = minX;
  }

  if (maxY < minY) {
    maxY = minY;
  }

  if (x < minX) {
    x = minX;
  }

  if (x > maxX) {
    x = maxX;
  }

  if (y < minY) {
    y = minY;
  }

  if (y > maxY) {
    y = maxY;
  }

  return Vec2{x, y};
}

void layoutSubTree(Tree &tree, NodeHandle node, Rect assignedRect);
void layoutStack(Tree &tree, NodeHandle node, Rect contentRect);
void layoutVertical(Tree &tree, NodeHandle node, Rect contentRect,
                    const LayoutStyle &style);
void layoutHorizontal(Tree &tree, NodeHandle node, Rect contentRect,
                      const LayoutStyle &style);
void layoutSplitContainer(Tree &tree, NodeHandle node);

f32 childDesiredHeight(
    const Tree &tree,
    NodeHandle child) {
  if (!tree.isValid(child) ||
      !tree.isVisible(child)) {
    return 0.0f;
  }

  const LayoutStyle &style =
      styleOrDefault(tree, child);

  if (style.fitContentY) {
    const f32 measured =
        measureNaturalHeight(
            tree,
            child);

    return clampFloat(
        measured,
        style.minSize.y,
        style.maxSize.y);
  }

  if (style.preferredSize.y > 0.0f) {
    return clampFloat(
        style.preferredSize.y,
        style.minSize.y,
        style.maxSize.y);
  }

  return style.minSize.y;
}

f32 measureNaturalHeight(
    const Tree &tree,
    NodeHandle node) {
  if (!tree.isValid(node) ||
      !tree.isVisible(node)) {
    return 0.0f;
  }

  const LayoutStyle &style =
      styleOrDefault(tree, node);

  f32 contentHeight = 0.0f;
  u32 visibleCount = 0;

  NodeHandle child =
      tree.firstChild(node);

  if (style.mode == LayoutMode::Vertical) {
    while (tree.isValid(child)) {
      if (tree.isVisible(child)) {
        if (visibleCount > 0) {
          contentHeight +=
              style.gap > 0.0f
                  ? style.gap
                  : 0.0f;
        }

        contentHeight +=
            childDesiredHeight(
                tree,
                child);

        ++visibleCount;
      }

      child = tree.nextSibling(child);
    }
  } else if (
      style.mode == LayoutMode::Horizontal ||
      style.mode == LayoutMode::Stack) {
    while (tree.isValid(child)) {
      if (tree.isVisible(child)) {
        const f32 childHeight =
            childDesiredHeight(
                tree,
                child);

        if (childHeight > contentHeight) {
          contentHeight = childHeight;
        }
      }

      child = tree.nextSibling(child);
    }
  }

  return contentHeight +
         style.padding.vertical();
}

void layoutStack(Tree &tree, NodeHandle node, Rect contentRect) {
  NodeHandle child = tree.firstChild(node);

  while (tree.isValid(child)) {
    if (tree.isVisible(child)) {
      layoutSubTree(tree, child, contentRect);
    }
    child = tree.nextSibling(child);
  }
}

void layoutVertical(Tree &tree, NodeHandle node, Rect contentRect,
                    const LayoutStyle &style) {
  const f32 gap = style.gap > 0.0f ? style.gap : 0.0f;

  f32 fixedMain = 0.0f;
  u32 visibleCount = 0;
  u32 flexibleCount = 0;

  NodeHandle child = tree.firstChild(node);

  while (tree.isValid(child)) {
    if (tree.isVisible(child)) {
      ++visibleCount;

      const LayoutStyle &childStyle = styleOrDefault(tree, child);

      if (childStyle.fitContentY) {
        fixedMain +=
            childDesiredHeight(
                tree,
                child);
      } else if (childStyle.preferredSize.y > 0.0f) {
        fixedMain += clampFloat(childStyle.preferredSize.y,
                                childStyle.minSize.y, childStyle.maxSize.y);
      } else {
        ++flexibleCount;
      }
    }

    child = tree.nextSibling(child);
  }

  if (visibleCount == 0) {
    return;
  }

  const f32 totalGap = gap * static_cast<f32>(visibleCount - 1);
  const f32 totalFixed = fixedMain + totalGap;
  const f32 remaining = contentRect.h - totalFixed;

  f32 flexibleShare = 0.0f;

  if (flexibleCount > 0 && remaining > 0.0f) {
    flexibleShare = remaining / static_cast<f32>(flexibleCount);
  }

  f32 y = contentRect.y;

  if (flexibleCount == 0 && totalFixed < contentRect.h) {
    if (style.verticalAlignment == Alignment::Center) {
      y += (contentRect.h - totalFixed) * 0.5f;
    } else if (style.verticalAlignment == Alignment::End) {
      y += contentRect.h - totalFixed;
    }
  }

  child = tree.firstChild(node);

  while (tree.isValid(child)) {
    if (tree.isVisible(child)) {
      const LayoutStyle &childStyle = styleOrDefault(tree, child);

      f32 mainSize;

      if (childStyle.fitContentY) {
        mainSize =
            childDesiredHeight(
                tree,
                child);
      } else if (childStyle.preferredSize.y > 0.0f) {
        mainSize = clampFloat(childStyle.preferredSize.y, childStyle.minSize.y,
                              childStyle.maxSize.y);
      } else {
        mainSize = clampFloat(flexibleShare, childStyle.minSize.y,
                              childStyle.maxSize.y);
      }

      f32 crossSize;

      if (style.horizontalAlignment == Alignment::Stretch) {
        crossSize = contentRect.w;
      } else {
        f32 desiredCross = childStyle.preferredSize.x;

        if (desiredCross < 0.0f) {
          desiredCross = childStyle.minSize.x;
        }

        f32 maxCross = childStyle.maxSize.x;

        if (contentRect.w < maxCross) {
          maxCross = contentRect.w;
        }

        crossSize = clampFloat(desiredCross, childStyle.minSize.x, maxCross);
      }

      f32 x = contentRect.x;

      if (style.horizontalAlignment == Alignment::Center) {
        x += (contentRect.w - crossSize) * 0.5f;
      } else if (style.horizontalAlignment == Alignment::End) {
        x += contentRect.w - crossSize;
      }

      layoutSubTree(tree, child, Rect{x, y, crossSize, mainSize});

      y += mainSize + gap;
    }

    child = tree.nextSibling(child);
  }
}

void layoutHorizontal(Tree &tree, NodeHandle node, Rect contentRect,
                      const LayoutStyle &style) {
  const f32 gap = style.gap > 0.0f ? style.gap : 0.0f;

  f32 fixedMain = 0.0f;
  u32 visibleCount = 0;
  u32 flexibleCount = 0;

  NodeHandle child = tree.firstChild(node);

  while (tree.isValid(child)) {
    if (tree.isVisible(child)) {
      ++visibleCount;

      const LayoutStyle &childStyle = styleOrDefault(tree, child);

      if (childStyle.preferredSize.x > 0.0f) {
        fixedMain += clampFloat(childStyle.preferredSize.x,
                                childStyle.minSize.x, childStyle.maxSize.x);
      } else {
        ++flexibleCount;
      }
    }

    child = tree.nextSibling(child);
  }

  if (visibleCount == 0) {
    return;
  }

  const f32 totalGap = gap * static_cast<f32>(visibleCount - 1);
  const f32 totalFixed = fixedMain + totalGap;
  const f32 remaining = contentRect.w - totalFixed;

  f32 flexibleShare = 0.0f;

  if (flexibleCount > 0 && remaining > 0.0f) {
    flexibleShare = remaining / static_cast<f32>(flexibleCount);
  }

  f32 x = contentRect.x;

  if (flexibleCount == 0 && totalFixed < contentRect.w) {
    if (style.verticalAlignment == Alignment::Center) {
      x += (contentRect.w - totalFixed) * 0.5f;
    } else if (style.verticalAlignment == Alignment::End) {
      x += contentRect.w - totalFixed;
    }
  }

  child = tree.firstChild(node);

  while (tree.isValid(child)) {
    if (tree.isVisible(child)) {
      const LayoutStyle &childStyle = styleOrDefault(tree, child);

      f32 mainSize;

      if (childStyle.preferredSize.x > 0.0f) {
        mainSize = clampFloat(childStyle.preferredSize.x, childStyle.minSize.x,
                              childStyle.maxSize.x);
      } else {
        mainSize = clampFloat(flexibleShare, childStyle.minSize.x,
                              childStyle.maxSize.x);
      }

      f32 crossSize;

      if (style.horizontalAlignment == Alignment::Stretch) {
        crossSize = contentRect.h;
      } else {
        f32 desiredCross = childStyle.preferredSize.y;

        if (desiredCross < 0.0f) {
          desiredCross = childStyle.minSize.y;
        }

        f32 maxCross = childStyle.maxSize.y;

        if (contentRect.h < maxCross) {
          maxCross = contentRect.h;
        }

        crossSize = clampFloat(desiredCross, childStyle.minSize.y, maxCross);
      }

      f32 y = contentRect.y;

      if (style.verticalAlignment == Alignment::Center) {
        y += (contentRect.h - crossSize) * 0.5f;
      } else if (style.verticalAlignment == Alignment::End) {
        y += contentRect.h - crossSize;
      }

      layoutSubTree(tree, child, Rect{x, y, mainSize, crossSize});

      x += mainSize + gap;
    }

    child = tree.nextSibling(child);
  }
}

void layoutSplitContainer(Tree &tree, NodeHandle node) {
  const SplitContainerState *state = tree.splitContainerState(node);

  if (!state) {
    return;
  }

  const detail::SplitGeometry geometry =
      detail::splitGeometry(tree, node, tree.rect(node));

  if (tree.isValid(state->firstPane) &&
      tree.isVisible(state->firstPane)) {
    layoutSubTree(tree, state->firstPane, geometry.first);
  }

  if (tree.isValid(state->secondPane) &&
      tree.isVisible(state->secondPane)) {
    layoutSubTree(tree, state->secondPane, geometry.second);
  }
}

void layoutSubTree(Tree &tree, NodeHandle node, Rect assignedRect) {
  if (!tree.isValid(node) || !tree.isVisible(node)) {
    return;
  }

  assignedRect.w = maxZero(assignedRect.w);
  assignedRect.h = maxZero(assignedRect.h);

  const Rect previousRect = tree.rect(node);

  const bool rectChanged = previousRect != assignedRect;

  if (rectChanged) {
    tree.markPaintDirty(node);
  }

  const bool selfDirty = tree.isLayoutDirty(node);
  const bool childrenDirty = tree.hasDirtyLayoutChildren(node);

  if (!rectChanged && !selfDirty && !childrenDirty) {
    return;
  }

  tree.setRect(node, assignedRect);

  const LayoutStyle &style = styleOrDefault(tree, node);

  const Rect contentRect{assignedRect.x + style.padding.left,
                         assignedRect.y + style.padding.top,
                         maxZero(assignedRect.w - style.padding.horizontal()),
                         maxZero(assignedRect.h - style.padding.vertical())};

  if (tree.type(node) == NodeType::SplitContainer) {
    layoutSplitContainer(tree, node);
  } else if (style.mode != LayoutMode::Manual && tree.childCount(node) > 0) {
    if (style.mode == LayoutMode::Stack) {
      layoutStack(tree, node, contentRect);
    } else if (style.mode == LayoutMode::Vertical) {
      layoutVertical(tree, node, contentRect, style);
    } else if (style.mode == LayoutMode::Horizontal) {
      layoutHorizontal(tree, node, contentRect, style);
    }
  }

  Vec2 contentSize = contentRect.size();

  NodeHandle child = tree.firstChild(node);

  while (tree.isValid(child)) {
    if (tree.isVisible(child)) {
      const Rect childRect = tree.rect(child);

      const f32 extentX = maxZero(childRect.maxX() - contentRect.x);
      const f32 extentY = maxZero(childRect.maxY() - contentRect.y);

      if (extentX > contentSize.x) {
        contentSize.x = extentX;
      }

      if (extentY > contentSize.y) {
        contentSize.y = extentY;
      }
    }

    child = tree.nextSibling(child);
  }

  if (LayoutResult *result = tree.layoutResult(node)) {
    result->rect = assignedRect;
    result->contentSize = contentSize;
    result->clipped = style.clip || style.scrollY;
    result->overflowed =
        contentSize.x > contentRect.w || contentSize.y > contentRect.h;
  }

  tree.setScrollOffsetY(node, tree.scrollOffsetY(node));
  tree.clearLayoutDirty(node);
}
} // namespace

void solveLayout(Tree &tree, NodeHandle root, const LayoutInput &input) {
  if (!tree.isValid(root)) {
    return;
  }

  const LayoutStyle &style = styleOrDefault(tree, root);

  Vec2 size = input.availableSize;

  if (style.preferredSize.x > 0.0f) {
    size.x = style.preferredSize.x;
  }

  if (style.preferredSize.y > 0.0f) {
    size.y = style.preferredSize.y;
  }

  size = clampSize(size, style.minSize, style.maxSize);

  Rect rootRect = tree.rect(root);
  rootRect.w = maxZero(size.x);
  rootRect.h = maxZero(size.y);

  layoutSubTree(tree, root, rootRect);
}
} // namespace octogui
