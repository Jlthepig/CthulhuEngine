#pragma once

#include "OctoGui/Tree.h"

namespace octogui::detail {

inline Rect contentRect(const Tree& tree, NodeHandle node, Rect rect) noexcept {
  const LayoutStyle* layout = tree.layoutStyle(node);

  if (!layout) {return rect;}

  rect.x += layout->padding.left;
  rect.y += layout->padding.top;
  rect.w -= layout->padding.horizontal();
  rect.h -= layout->padding.vertical();

  if (rect.w < 0.0f) {rect.w = 0.0f;}
  if (rect.h < 0.0f) {rect.h = 0.0f;}

  return rect;
}

inline Vec2 visualOffset(const Tree& tree, NodeHandle node) noexcept {
  Vec2 offset{};

  NodeHandle current = tree.parent(node);

  while (tree.isValid(current)) {
    const LayoutStyle* layout = tree.layoutStyle(current);

    if (layout && layout->scrollY) {
      offset.y -= tree.scrollOffsetY(current);
    }

    current = tree.parent(current);
  }

  return offset;
}

inline Rect visualRect(const Tree& tree, NodeHandle node) noexcept {
  return tree.rect(node).translate(visualOffset(tree, node));
}

inline Rect visualContentRect(const Tree& tree, NodeHandle node) noexcept {
  return contentRect(tree, node, visualRect(tree, node));
}

} // namespace octogui::detail