#include "Systems/InteractionSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/SplitLayoutInternal.h"
#include "Systems/Interaction/InteractionInternal.h"
#include "Internal/ZOrder.h"

namespace octogui {

NodeHandle InteractionSystem::hitTest(const Tree &tree, NodeHandle node,
                                      Vec2 point, Vec2 offset) {
  if (!tree.isValid(node) || !tree.isVisible(node)) {
    return NodeHandle{};
  }

  const Rect rect = tree.rect(node).translate(offset);

  if (!rect.contains(point)) {
    return NodeHandle{};
  }

  const LayoutStyle *style = tree.layoutStyle(node);

  Vec2 childOffset = offset;
  bool testChildren = true;

  if (style && style->scrollY) {
    const Rect viewport = detail::contentRect(tree, node, rect);

    if (!viewport.contains(point)) {
      testChildren = false;
    }

    childOffset.y -= tree.scrollOffsetY(node);
  }

  if (testChildren) {
    const detail::ZOrderRange range =
        detail::appendChildrenByZ(tree, node, zOrderScratch);

    for (usize i = range.end; i > range.begin; --i) {
      const NodeHandle child = zOrderScratch[i - 1];
      const NodeHandle hit = hitTest(tree, child, point, childOffset);

      if (tree.isValid(hit)) {
        return hit;
      }
    }
  }
  return node;
}

NodeHandle InteractionSystem::hitTestScrollbar(const Tree &tree,
                                               NodeHandle node, Vec2 point,
                                               Vec2 offset) {
  if (!tree.isValid(node) || !tree.isVisible(node)) {
    return NodeHandle{};
  }

  const Rect rect = tree.rect(node).translate(offset);

  if (!rect.contains(point)) {
    return NodeHandle{};
  }

  const LayoutStyle *style = tree.layoutStyle(node);

  Vec2 childOffset = offset;
  bool testChildren = true;

  if (style && style->scrollY) {
    const Rect viewport = detail::contentRect(tree, node, rect);

    if (!viewport.contains(point)) {
      testChildren = false;
    }

    childOffset.y -= tree.scrollOffsetY(node);
  }

  if (testChildren) {
    const detail::ZOrderRange range =
        detail::appendChildrenByZ(tree, node, zOrderScratch);

    for (usize i = range.end; i > range.begin; --i) {
      const NodeHandle child = zOrderScratch[i - 1];
      const NodeHandle hit = hitTestScrollbar(tree, child, point, childOffset);

      if (tree.isValid(hit)) {
        return hit;
      }
    }
  }

  if (style && style->scrollY) {
    const detail::VerticalScrollbarGeometry geometry =
        interaction_detail::scrollbarGeometry(tree, node, rect);

    if (geometry.visible && geometry.track.contains(point)) {
      return node;
    }
  }

  return NodeHandle{};
}

NodeHandle InteractionSystem::hitTestSplitter(const Tree &tree,
                                              NodeHandle node, Vec2 point,
                                              Vec2 offset) {
  if (!tree.isValid(node) || !tree.isVisible(node)) {
    return {};
  }

  const Rect rect = tree.rect(node).translate(offset);

  if (!rect.contains(point)) {
    return {};
  }

  const LayoutStyle *style = tree.layoutStyle(node);
  Vec2 childOffset = offset;
  bool testChildren = true;

  if (style && style->scrollY) {
    const Rect viewport = detail::contentRect(tree, node, rect);

    if (!viewport.contains(point)) {
      testChildren = false;
    }

    childOffset.y -= tree.scrollOffsetY(node);
  }

  if (testChildren) {
    const detail::ZOrderRange range =
        detail::appendChildrenByZ(tree, node, zOrderScratch);

    for (usize i = range.end; i > range.begin; --i) {
      const NodeHandle child = zOrderScratch[i - 1];
      const NodeHandle hit = hitTestSplitter(tree, child, point, childOffset);

      if (tree.isValid(hit)) {
        return hit;
      }
    }
  }

  if (tree.type(node) == NodeType::SplitContainer) {
    const detail::SplitGeometry geometry =
        detail::splitGeometry(tree, node, rect);

    if (!geometry.hit.isEmpty() && geometry.hit.contains(point)) {
      return node;
    }
  }

  return {};
}

bool InteractionSystem::isEnabledBranch(const Tree &tree,
                                        NodeHandle node) const {
  if (!tree.isValid(node)) {
    return false;
  }

  NodeHandle current = node;

  while (tree.isValid(current)) {
    if (!tree.isEnabled(current)) {
      return false;
    }

    current = tree.parent(current);
  }

  return true;
}

NodeHandle InteractionSystem::findInteractive(const Tree &tree,
                                              NodeHandle node) const {
  NodeHandle current = node;

  while (tree.isValid(current)) {
    if (tree.isEnabled(current) &&
        hasFlag(tree.flags(current), NodeFlags::Interactive)) {
      return current;
    }

    current = tree.parent(current);
  }

  return NodeHandle{};
}

void InteractionSystem::markStateChangeDirty(Tree &tree, NodeHandle previous,
                                             NodeHandle current) {
  if (previous == current) {
    return;
  }

  if (tree.isValid(previous)) {
    tree.markPaintDirty(previous);
  }
  if (tree.isValid(current)) {
    tree.markPaintDirty(current);
  }
}

} // namespace octogui
