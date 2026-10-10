#pragma once

#include "Systems/InteractionSystem.h"

#include "Internal/WidgetMetrics.h"

namespace octogui::interaction_detail {

inline constexpr f32 ScrollWheelStep = 40.0f;

inline detail::VerticalScrollbarGeometry scrollbarGeometry(
    const Tree& tree,
    NodeHandle node,
    Rect visualRect) {
  const LayoutStyle* style = tree.layoutStyle(node);
  const LayoutResult* result = tree.layoutResult(node);

  if (!style || !style->scrollY || !result) {
    return {};
  }

  const Rect viewport =
      detail::verticalScrollbarViewport(
          visualRect,
          style->padding);

  return detail::verticalScrollbarGeometry(
      viewport,
      result->contentSize.y,
      tree.scrollOffsetY(node),
      tree.maxScrollOffsetY(node));
}

inline bool isTextEditingType(NodeType type) noexcept {
  return type == NodeType::TextInput ||
         type == NodeType::NumericInput;
}

} // namespace octogui::interaction_detail
