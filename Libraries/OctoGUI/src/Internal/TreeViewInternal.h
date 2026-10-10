#pragma once

#include "OctoGui/Tree.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/WidgetMetrics.h"

namespace octogui::detail {

inline TreeViewItemSlot* treeViewItem(
    TreeViewState& state,
    TreeViewItemId id) noexcept {
  if (id.index >= state.items.size()) {
    return nullptr;
  }

  TreeViewItemSlot& item =
      state.items[id.index];

  if (!item.alive ||
      item.generation != id.generation) {
    return nullptr;
  }

  return &item;
}

inline const TreeViewItemSlot* treeViewItem(
    const TreeViewState& state,
    TreeViewItemId id) noexcept {
  if (id.index >= state.items.size()) {
    return nullptr;
  }

  const TreeViewItemSlot& item =
      state.items[id.index];

  if (!item.alive ||
      item.generation != id.generation) {
    return nullptr;
  }

  return &item;
}

inline void rebuildTreeViewVisibleItems(
    TreeViewState& state) {
  if (!state.visibleItemsDirty) {
    return;
  }

  state.visibleItems.clear();

  if (state.itemCount == 0 ||
      !state.firstRoot) {
    state.visibleItemsDirty = false;
    return;
  }

  if (state.visibleItems.capacity() <
      state.itemCount) {
    state.visibleItems.reserve(
        state.itemCount);
  }

  TreeViewItemId current =
      state.firstRoot;

  u32 depth = 0;
  u32 visited = 0;

  while (current &&
         visited < state.itemCount) {
    TreeViewItemSlot* item =
        treeViewItem(
            state,
            current);

    if (!item) {
      break;
    }

    state.visibleItems.push_back(
        TreeViewVisibleItem{
            current,
            depth});

    ++visited;

    if (item->expanded &&
        item->firstChild) {
      current =
          item->firstChild;

      ++depth;

      continue;
    }

    while (current) {
      item =
          treeViewItem(
              state,
              current);

      if (!item) {
        current = {};
        break;
      }

      if (item->nextSibling) {
        current =
            item->nextSibling;

        break;
      }

      current =
          item->parent;

      if (current &&
          depth > 0) {
        --depth;
      }
    }
  }

  state.visibleItemsDirty = false;
}

inline f32 treeViewContentHeight(
    TreeViewState& state) {
  rebuildTreeViewVisibleItems(
      state);

  return static_cast<f32>(
             state.visibleItems.size()) *
         TreeViewRowHeight;
}

inline f32 treeViewMaxScrollY(
    TreeViewState& state,
    const Rect& content) {
  const f32 contentHeight =
      treeViewContentHeight(
          state);

  f32 maxScroll =
      contentHeight -
      content.h;

  if (maxScroll < 0.0f) {
    maxScroll = 0.0f;
  }

  return maxScroll;
}

inline void clampTreeViewScrollY(
    TreeViewState& state,
    const Rect& content) {
  const f32 maxScroll =
      treeViewMaxScrollY(
          state,
          content);

  if (state.scrollY < 0.0f) {
    state.scrollY = 0.0f;
  }

  if (state.scrollY > maxScroll) {
    state.scrollY = maxScroll;
  }
}

inline VerticalScrollbarGeometry
treeViewScrollbarGeometry(
    TreeViewState& state,
    const Tree& tree,
    NodeHandle node) {
  const Rect content =
        visualContentRect(
          tree,
          node);

  const Rect outer =
        visualRect(
          tree,
          node);

  const LayoutStyle* layout =
      tree.layoutStyle(node);

  if (!layout ||
      content.isEmpty()) {
    return {};
  }

  const Rect viewport =
      verticalScrollbarViewport(
          outer,
          layout->padding);

  const f32 contentHeight =
      treeViewContentHeight(
          state);

  const f32 maxScroll =
      treeViewMaxScrollY(
          state,
          content);

  return verticalScrollbarGeometry(
      viewport,
      contentHeight,
      state.scrollY,
      maxScroll);
}

inline void ensureTreeViewSelectionVisible(
    TreeViewState& state,
    const Rect& content) {
  if (!state.scrollToSelectionPending ||
      !state.selected) {
    return;
  }

  rebuildTreeViewVisibleItems(
      state);

  for (usize i = 0;
       i < state.visibleItems.size();
       ++i) {
    if (state.visibleItems[i].item !=
        state.selected) {
      continue;
    }

    const f32 top =
        static_cast<f32>(i) *
        TreeViewRowHeight;

    const f32 bottom =
        top +
        TreeViewRowHeight;

    if (top < state.scrollY) {
      state.scrollY = top;
    } else if (bottom >
               state.scrollY + content.h) {
      state.scrollY =
          bottom -
          content.h;
    }

    clampTreeViewScrollY(
        state,
        content);

    state.scrollToSelectionPending =
        false;

    return;
  }
}

inline Rect treeViewRowRect(
    const Rect& content,
    usize rowIndex,
    f32 scrollY) noexcept {
  return Rect{
      content.x,
      content.y +
          static_cast<f32>(rowIndex) *
              TreeViewRowHeight -
          scrollY,
      content.w,
      TreeViewRowHeight};
}

inline TreeViewItemId treeViewItemAt(
    TreeViewState& state,
    const Rect& content,
    Vec2 position) {
  rebuildTreeViewVisibleItems(
      state);

  if (!content.contains(position) ||
      state.visibleItems.empty()) {
    return {};
  }

  const f32 localY =
      position.y -
      content.y +
      state.scrollY;

  if (localY < 0.0f) {
    return {};
  }

  const usize rowIndex =
      static_cast<usize>(
          localY /
          TreeViewRowHeight);

  if (rowIndex >=
      state.visibleItems.size()) {
    return {};
  }

  return state
      .visibleItems[rowIndex]
      .item;
}

inline const TreeViewVisibleItem*
treeViewVisibleItemAt(
    TreeViewState& state,
    const Rect& content,
    Vec2 position) {
  rebuildTreeViewVisibleItems(
      state);

  if (!content.contains(position) ||
      state.visibleItems.empty()) {
    return nullptr;
  }

  const f32 localY =
      position.y -
      content.y +
      state.scrollY;

  if (localY < 0.0f) {
    return nullptr;
  }

  const usize rowIndex =
      static_cast<usize>(
          localY /
          TreeViewRowHeight);

  if (rowIndex >=
      state.visibleItems.size()) {
    return nullptr;
  }

  return &
      state.visibleItems[rowIndex];
}

inline Rect treeViewChevronRect(
    const Rect& content,
    const TreeViewVisibleItem& visible,
    usize rowIndex,
    f32 scrollY) noexcept {
  const Rect row =
      treeViewRowRect(
          content,
          rowIndex,
          scrollY);

  return Rect{
      row.x +
          static_cast<f32>(
              visible.depth) *
              TreeViewIndentWidth,
      row.y,
      TreeViewChevronAreaWidth,
      row.h};
}

inline bool treeViewPointIsChevron(
    TreeViewState& state,
    const Rect& content,
    Vec2 position) {
  rebuildTreeViewVisibleItems(
      state);

  if (!content.contains(position)) {
    return false;
  }

  const f32 localY =
      position.y -
      content.y +
      state.scrollY;

  if (localY < 0.0f) {
    return false;
  }

  const usize rowIndex =
      static_cast<usize>(
          localY /
          TreeViewRowHeight);

  if (rowIndex >=
      state.visibleItems.size()) {
    return false;
  }

  const TreeViewVisibleItem& visible =
      state.visibleItems[rowIndex];

  const TreeViewItemSlot* item =
      treeViewItem(
          state,
          visible.item);

  if (!item ||
      !item->firstChild) {
    return false;
  }

  const Rect chevron =
      treeViewChevronRect(
          content,
          visible,
          rowIndex,
          state.scrollY);

  return chevron.contains(
      position);
}

} // namespace octogui::detail