#include "Systems/InteractionSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Systems/Interaction/InteractionInternal.h"
#include "Internal/WidgetMetrics.h"

namespace octogui {

void InteractionSystem::beginScrollbarInteraction(Tree &tree, NodeHandle node,
                                                  Vec2 mousePosition) {
  if (!tree.isValid(node)) {
    return;
  }

    const Rect visualRect =
      detail::visualRect(tree, node);

  const detail::VerticalScrollbarGeometry geometry =
      interaction_detail::scrollbarGeometry(tree, node, visualRect);

  if (!geometry.visible) {
    return;
  }

  if (geometry.thumb.contains(mousePosition)) {
    interaction.scrollbarActive = node;
    scrollbarGrabOffsetY = mousePosition.y - geometry.thumb.y;
    tree.markPaintDirty(node);
    return;
  }

  if (!geometry.track.contains(mousePosition)) {
    return;
  }

  const f32 pageAmount = geometry.track.h * detail::ScrollbarPageFactor;

  f32 offset = tree.scrollOffsetY(node);

  if (mousePosition.y < geometry.thumb.y) {
    offset -= pageAmount;
  } else if (mousePosition.y >= geometry.thumb.maxY()) {
    offset += pageAmount;
  }

  tree.setScrollOffsetY(node, offset);
}

void InteractionSystem::updateScrollbarDrag(Tree &tree, NodeHandle node,
                                            Vec2 mousePosition) {
  if (!tree.isValid(node)) {
    return;
  }

    const Rect visualRect =
      detail::visualRect(tree, node);

  const detail::VerticalScrollbarGeometry geometry =
      interaction_detail::scrollbarGeometry(tree, node, visualRect);

  if (!geometry.visible) {
    return;
  }

  const f32 travel = geometry.track.h - geometry.thumb.h;

  if (travel <= 0.0f) {
    return;
  }

  f32 thumbY = mousePosition.y - scrollbarGrabOffsetY;

  const f32 minY = geometry.track.y;
  const f32 maxY = geometry.track.y + travel;

  if (thumbY < minY) {
    thumbY = minY;
  }
  if (thumbY > maxY) {
    thumbY = maxY;
  }

  const f32 t = (thumbY - geometry.track.y) / travel;

  tree.setScrollOffsetY(node, tree.maxScrollOffsetY(node) * t);
}

bool InteractionSystem::applyWheelScroll(Tree &tree, NodeHandle hit,
                                         f32 wheelY) {
  if (!tree.isValid(hit) || wheelY == 0.0f) {
    return false;
  }

  f32 remaining = -wheelY * interaction_detail::ScrollWheelStep;
  bool changed = false;

  NodeHandle current = hit;

  while (tree.isValid(current) && remaining != 0.0f) {
    const LayoutStyle *style = tree.layoutStyle(current);

    if (style && style->scrollY && isEnabledBranch(tree, current)) {
      const f32 oldOffset = tree.scrollOffsetY(current);
      const f32 maxOffset = tree.maxScrollOffsetY(current);

      f32 newOffset = oldOffset + remaining;

      if (newOffset < 0.0f) {
        newOffset = 0.0f;
      }

      if (newOffset > maxOffset) {
        newOffset = maxOffset;
      }

      const f32 consumed = newOffset - oldOffset;

      if (consumed != 0.0f) {
        tree.setScrollOffsetY(current, newOffset);
        remaining -= consumed;
        changed = true;
      }
    }

    current = tree.parent(current);
  }

  return changed;
}

} // namespace octogui
