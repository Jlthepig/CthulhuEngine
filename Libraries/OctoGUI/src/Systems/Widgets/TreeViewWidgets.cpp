#include <vector>

#include "Systems/EventSystem.h"
#include "Systems/StyleSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/TreeViewInternal.h"
namespace octogui {

namespace {

using detail::treeViewItem;

void advanceGeneration(TreeViewItemSlot& item) noexcept {
    ++item.generation;

    if (item.generation == 0) {
        item.generation = 1;
    }
}

TreeViewItemId allocateTreeViewItem(TreeViewState& state) {
    u32 index = 0;

    if (!state.freeItems.empty()) {
        index =
            state.freeItems.back();

        state.freeItems.pop_back();
    } else {
        index =
            static_cast<u32>(
                state.items.size());

        state.items.emplace_back();
    }

    TreeViewItemSlot& item =
        state.items[index];

    item.label.clear();

    item.userValue = 0;

    item.parent = {};
    item.firstChild = {};
    item.lastChild = {};

    item.previousSibling = {};
    item.nextSibling = {};

    if (item.generation == 0) {
        item.generation = 1;
    }

    item.alive = true;
    item.expanded = true;

    return TreeViewItemId{
        index,
        item.generation};
}

void releaseTreeViewItem(TreeViewState& state, TreeViewItemId id) {
    TreeViewItemSlot* item =
        treeViewItem(state, id);

    if (!item) {
        return;
    }

    item->label.clear();

    item->userValue = 0;

    item->parent = {};
    item->firstChild = {};
    item->lastChild = {};

    item->previousSibling = {};
    item->nextSibling = {};

    item->alive = false;
    item->expanded = true;

    advanceGeneration(*item);

    state.freeItems.push_back(
        id.index);
}

void unlinkTreeViewItem(TreeViewState& state, TreeViewItemId id) {
    TreeViewItemSlot* item =
        treeViewItem(state, id);

    if (!item) {
        return;
    }

    TreeViewItemSlot* previous =
        treeViewItem(
            state,
            item->previousSibling);

    TreeViewItemSlot* next =
        treeViewItem(
            state,
            item->nextSibling);

    TreeViewItemSlot* parent =
        treeViewItem(
            state,
            item->parent);

    if (previous) {
        previous->nextSibling =
            item->nextSibling;
    } else if (parent) {
        parent->firstChild =
            item->nextSibling;
    } else {
        state.firstRoot =
            item->nextSibling;
    }

    if (next) {
        next->previousSibling =
            item->previousSibling;
    } else if (parent) {
        parent->lastChild =
            item->previousSibling;
    } else {
        state.lastRoot =
            item->previousSibling;
    }

    item->previousSibling = {};
    item->nextSibling = {};
}

} // namespace

NodeHandle WidgetSystem::createTreeView(Tree& tree, StyleSystem& styles, NodeHandle parent) {
  NodeHandle handle =
      createNode(
          tree,
          styles,
          NodeType::TreeView,
          parent);

    tree.setInteractive(handle, true);
    tree.setFocusable(handle, true);

  LayoutStyle* layout =
      tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize =
        Vec2{240.0f, 240.0f};

    layout->minSize =
        Vec2{120.0f, 80.0f};

    layout->padding =
        Padding{
            4.0f,
            4.0f,
            14.0f,
            4.0f};
  }

  return handle;
}

TreeViewItemId WidgetSystem::addTreeViewItem(Tree& tree, NodeHandle handle, TreeViewItemId parent, std::string_view text, u64 userValue) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state) {
    return {};
  }

    if (parent &&
            !treeViewItem(
                    *state,
                    parent)) {
        return {};
    }

  const TreeViewItemId id =
      allocateTreeViewItem(
          *state);

  TreeViewItemSlot* item =
      treeViewItem(
          *state,
          id);

  if (!item) {
    return {};
  }

    TreeViewItemSlot* parentItem =
            parent
                    ? treeViewItem(
                                *state,
                                parent)
                    : nullptr;

    if (parent && !parentItem) {
        return {};
    }

  item->label.assign(
      text.data(),
      text.size());

  item->userValue =
      userValue;

  item->parent =
      parent;

  if (parentItem) {
    if (parentItem->lastChild) {
      TreeViewItemSlot* last =
          treeViewItem(
              *state,
              parentItem->lastChild);

      if (last) {
        last->nextSibling =
            id;

        item->previousSibling =
            parentItem->lastChild;
      }
    } else {
      parentItem->firstChild =
          id;
    }

    parentItem->lastChild =
        id;
  } else {
    if (state->lastRoot) {
      TreeViewItemSlot* last =
          treeViewItem(
              *state,
              state->lastRoot);

      if (last) {
        last->nextSibling =
            id;

        item->previousSibling =
            state->lastRoot;
      }
    } else {
      state->firstRoot =
          id;
    }

    state->lastRoot =
        id;
  }

  ++state->itemCount;

  state->visibleItemsDirty =
      true;

  tree.markPaintDirty(
      handle);

  return id;
}

bool WidgetSystem::removeTreeViewItem(Tree& tree, EventSystem& events,
                                                                            NodeHandle handle,
                                                                            TreeViewItemId item) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state ||
      !treeViewItem(*state, item)) {
    return false;
  }

    const TreeViewItemId oldSelection =
            state->selected;

  unlinkTreeViewItem(
      *state,
      item);

  std::vector<TreeViewItemId> stack;

  stack.reserve(16);
  stack.push_back(item);

  while (!stack.empty()) {
    const TreeViewItemId currentId =
        stack.back();

    stack.pop_back();

    TreeViewItemSlot* current =
        treeViewItem(
            *state,
            currentId);

    if (!current) {
      continue;
    }

    TreeViewItemId child =
        current->firstChild;

    while (child) {
      TreeViewItemSlot* childItem =
          treeViewItem(
              *state,
              child);

      if (!childItem) {
        break;
      }

      const TreeViewItemId next =
          childItem->nextSibling;

      stack.push_back(
          child);

      child = next;
    }

    if (state->selected ==
        currentId) {
      state->selected = {};
    }

    if (state->hovered ==
        currentId) {
      state->hovered = {};
    }

    if (state->pressed ==
        currentId) {
      state->pressed = {};
            state->pressedChevron = false;
    }

    releaseTreeViewItem(
        *state,
        currentId);

    if (state->itemCount > 0) {
      --state->itemCount;
    }
  }

  state->visibleItemsDirty =
      true;

  tree.markPaintDirty(
      handle);

    if (oldSelection &&
            !state->selected) {
        events.pushSelectionChanged(
                handle,
                oldSelection,
                {});
    }

  return true;
}

void WidgetSystem::clearTreeView(Tree& tree, EventSystem& events,
                                                                 NodeHandle handle) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state) {
    return;
  }

    const TreeViewItemId oldSelection =
            state->selected;

  state->freeItems.clear();

  state->freeItems.reserve(
      state->items.size());

  for (u32 i = 0;
       i < state->items.size();
       ++i) {
    TreeViewItemSlot& item =
        state->items[i];

    item.label.clear();

    item.userValue = 0;

    item.parent = {};
    item.firstChild = {};
    item.lastChild = {};

    item.previousSibling = {};
    item.nextSibling = {};

    item.alive = false;
    item.expanded = true;

    advanceGeneration(item);

    state->freeItems.push_back(i);
  }

  state->visibleItems.clear();

  state->firstRoot = {};
  state->lastRoot = {};

  state->selected = {};
  state->hovered = {};
  state->pressed = {};
    state->pressedChevron = false;

  state->itemCount = 0;

  state->scrollY = 0.0f;

    state->scrollbarDragOffsetY = 0.0f;
    state->scrollbarHovered = false;
    state->scrollbarDragging = false;
    state->scrollToSelectionPending = false;

  state->visibleItemsDirty =
      true;

  tree.markPaintDirty(
      handle);

    if (oldSelection) {
        events.pushSelectionChanged(
                handle,
                oldSelection,
                {});
    }
}

bool WidgetSystem::treeViewItemExists(const Tree& tree, NodeHandle handle, TreeViewItemId item) const noexcept {
  const TreeViewState* state =
      tree.treeViewState(handle);

  return state &&
         treeViewItem(
             *state,
             item);
}

u32 WidgetSystem::treeViewItemCount(const Tree& tree, NodeHandle handle) const noexcept {
  const TreeViewState* state =
      tree.treeViewState(handle);

  return state
      ? state->itemCount
      : 0;
}

TreeViewItemId WidgetSystem::treeViewSelectedItem(const Tree& tree, NodeHandle handle) const noexcept {
  const TreeViewState* state =
      tree.treeViewState(handle);

  if (!state ||
      !treeViewItem(
          *state,
          state->selected)) {
    return {};
  }

  return state->selected;
}

void WidgetSystem::setTreeViewSelectedItem(Tree& tree, EventSystem& events, NodeHandle handle, TreeViewItemId item) {
  TreeViewState* state = tree.treeViewState(handle);

  if (!state) {
    return;
  }

  if (item && !treeViewItem(*state, item)) {
    return;
  }

  const TreeViewItemId oldSelection = state->selected;

  if (oldSelection == item) {
    return;
  }

  state->selected = item;
    state->scrollToSelectionPending = true;

  tree.markPaintDirty(handle);

  events.pushSelectionChanged(handle, oldSelection, item);
}

std::string_view WidgetSystem::treeViewItemText(
        const Tree& tree,
        NodeHandle handle,
        TreeViewItemId item) const noexcept {
    const TreeViewState* state = tree.treeViewState(handle);
    const TreeViewItemSlot* slot = state ? treeViewItem(*state, item) : nullptr;

    return slot ? std::string_view(slot->label) : std::string_view{};
}

void WidgetSystem::setTreeViewItemText(
        Tree& tree,
        NodeHandle handle,
        TreeViewItemId item,
        std::string_view text) {
    TreeViewState* state = tree.treeViewState(handle);
    TreeViewItemSlot* slot = state ? treeViewItem(*state, item) : nullptr;

    if (!slot || slot->label == text) {
        return;
    }

    slot->label.assign(text);
    tree.markPaintDirty(handle);
}

u64 WidgetSystem::treeViewItemUserValue(
        const Tree& tree,
        NodeHandle handle,
        TreeViewItemId item) const noexcept {
    const TreeViewState* state = tree.treeViewState(handle);
    const TreeViewItemSlot* slot = state ? treeViewItem(*state, item) : nullptr;

    return slot ? slot->userValue : 0;
}

void WidgetSystem::setTreeViewItemUserValue(
        Tree& tree,
        NodeHandle handle,
        TreeViewItemId item,
        u64 value) {
    TreeViewState* state = tree.treeViewState(handle);
    TreeViewItemSlot* slot = state ? treeViewItem(*state, item) : nullptr;

    if (!slot) {
        return;
    }

    slot->userValue = value;
}

bool WidgetSystem::isTreeViewItemExpanded(
        const Tree& tree,
        NodeHandle handle,
        TreeViewItemId item) const noexcept {
    const TreeViewState* state = tree.treeViewState(handle);
    const TreeViewItemSlot* slot = state ? treeViewItem(*state, item) : nullptr;

    return slot ? slot->expanded : false;
}

void WidgetSystem::setTreeViewItemExpanded(
        Tree& tree,
        NodeHandle handle,
        TreeViewItemId item,
        bool expanded) {
    TreeViewState* state = tree.treeViewState(handle);
    TreeViewItemSlot* slot = state ? treeViewItem(*state, item) : nullptr;

    if (!slot || slot->expanded == expanded) {
        return;
    }

    slot->expanded = expanded;
    state->visibleItemsDirty = true;
    tree.markPaintDirty(handle);
}

void WidgetSystem::updateTreeViewHover(
    Tree& tree,
    NodeHandle handle,
    Vec2 mousePosition) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state) {
    return;
  }

  const Rect content =
    detail::visualContentRect(
          tree,
          handle);

  const auto scrollbar =
      detail::treeViewScrollbarGeometry(
          *state,
          tree,
          handle);

  const bool scrollbarHovered =
      scrollbar.visible &&
      scrollbar.track.contains(
          mousePosition);

  const bool scrollbarChanged =
      state->scrollbarHovered !=
      scrollbarHovered;

  state->scrollbarHovered =
      scrollbarHovered;

  TreeViewItemId hovered{};

  if (!scrollbarHovered) {
    hovered =
        detail::treeViewItemAt(
            *state,
            content,
            mousePosition);
  }

  if (state->hovered != hovered ||
      scrollbarChanged) {
    state->hovered = hovered;

    tree.markPaintDirty(
        handle);
  }
}

void WidgetSystem::clearTreeViewHover(
    Tree& tree,
    NodeHandle handle) {
  TreeViewState* state =
      tree.treeViewState(handle);

    if (!state ||
            (!state->hovered &&
             !state->scrollbarHovered)) {
    return;
  }

  state->hovered = {};
    state->scrollbarHovered = false;

  tree.markPaintDirty(
      handle);
}

bool WidgetSystem::scrollTreeView(
        Tree& tree,
        NodeHandle handle,
        f32 wheelDelta) {
    TreeViewState* state =
            tree.treeViewState(handle);

    if (!state ||
            wheelDelta == 0.0f) {
        return false;
    }

    const Rect content =
            detail::visualContentRect(
                    tree,
                    handle);

    if (content.isEmpty()) {
        return false;
    }

    const f32 maxScroll =
            detail::treeViewMaxScrollY(
                    *state,
                    content);

    if (maxScroll <= 0.0f) {
        return false;
    }

    const f32 oldScroll =
            state->scrollY;

    state->scrollY -=
            wheelDelta *
            detail::TreeViewRowHeight *
            3.0f;

    detail::clampTreeViewScrollY(
            *state,
            content);

    if (state->scrollY ==
            oldScroll) {
        return false;
    }

    tree.markPaintDirty(
            handle);

    return true;
}

bool WidgetSystem::updateTreeViewScrollbarDrag(
        Tree& tree,
        NodeHandle handle,
        Vec2 mousePosition) {
    TreeViewState* state =
            tree.treeViewState(handle);

    if (!state ||
            !state->scrollbarDragging) {
        return false;
    }

    const Rect content =
            detail::visualContentRect(
                    tree,
                    handle);

    const auto scrollbar =
            detail::treeViewScrollbarGeometry(
                    *state,
                    tree,
                    handle);

    if (!scrollbar.visible) {
        return true;
    }

    const f32 maxScroll =
            detail::treeViewMaxScrollY(
                    *state,
                    content);

    const f32 travel =
            scrollbar.track.h -
            scrollbar.thumb.h;

    if (travel <= 0.0f ||
            maxScroll <= 0.0f) {
        return true;
    }

    f32 thumbY =
            mousePosition.y -
            state->scrollbarDragOffsetY;

    const f32 minY = scrollbar.track.y;
    const f32 maxY = scrollbar.track.maxY() - scrollbar.thumb.h;

    if (thumbY < minY) {
        thumbY = minY;
    }
    if (thumbY > maxY) {
        thumbY = maxY;
    }

    state->scrollY =
            (thumbY - minY) /
            travel *
            maxScroll;

    tree.markPaintDirty(handle);
    return true;
}

void WidgetSystem::updateTreeViewPress(
    Tree& tree,
    NodeHandle handle,
    Vec2 mousePosition) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state ||
      !state->scrollbarDragging) {
    return;
  }

  const Rect content =
    detail::visualContentRect(
          tree,
          handle);

  const auto scrollbar =
      detail::treeViewScrollbarGeometry(
          *state,
          tree,
          handle);

  if (!scrollbar.visible) {
    state->scrollbarDragging = false;
    return;
  }

  const f32 travel =
      scrollbar.track.h -
      scrollbar.thumb.h;

  const f32 maxScroll =
      detail::treeViewMaxScrollY(
          *state,
          content);

  if (travel <= 0.0f ||
      maxScroll <= 0.0f) {
    state->scrollY = 0.0f;
    return;
  }

  f32 thumbY =
      mousePosition.y -
      state->scrollbarDragOffsetY;

  f32 t =
      (thumbY -
       scrollbar.track.y) /
      travel;

  if (t < 0.0f) {
    t = 0.0f;
  }

  if (t > 1.0f) {
    t = 1.0f;
  }

  const f32 newScroll =
      t * maxScroll;

  if (state->scrollY ==
      newScroll) {
    return;
  }

  state->scrollY =
      newScroll;

  tree.markPaintDirty(
      handle);
}

void WidgetSystem::beginTreeViewPress(
    Tree& tree,
    NodeHandle handle,
    Vec2 mousePosition) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state) {
    return;
  }

  const Rect content =
    detail::visualContentRect(
          tree,
          handle);

    const auto scrollbar =
            detail::treeViewScrollbarGeometry(
                    *state,
                    tree,
                    handle);

    if (scrollbar.visible) {
        if (scrollbar.thumb.contains(
                        mousePosition)) {
            state->scrollbarDragging = true;
            state->scrollbarDragOffsetY =
                    mousePosition.y -
                    scrollbar.thumb.y;
            state->pressed = {};
            state->pressedChevron = false;
            tree.markPaintDirty(handle);
            return;
        }

        if (scrollbar.track.contains(
                        mousePosition)) {
            const f32 page =
                    content.h *
                    detail::ScrollbarPageFactor;

            if (mousePosition.y < scrollbar.thumb.y) {
                state->scrollY -= page;
            } else if (mousePosition.y > scrollbar.thumb.maxY()) {
                state->scrollY += page;
            }

            detail::clampTreeViewScrollY(
                    *state,
                    content);

            state->pressed = {};
            state->pressedChevron = false;
            tree.markPaintDirty(handle);
            return;
        }
    }

    const TreeViewItemId item =
      detail::treeViewItemAt(
          *state,
          content,
          mousePosition);

  state->pressed =
      item;

  state->pressedChevron =
      item &&
      detail::treeViewPointIsChevron(
          *state,
          content,
          mousePosition);
}

void WidgetSystem::cancelTreeViewPress(
    Tree& tree,
    NodeHandle handle) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state) {
    return;
  }

  state->pressed = {};
  state->pressedChevron = false;
    state->scrollbarDragging = false;
    state->scrollbarDragOffsetY = 0.0f;
}

void WidgetSystem::endTreeViewPress(
    Tree& tree,
    EventSystem& events,
    NodeHandle handle,
    Vec2 mousePosition) {
  TreeViewState* state =
      tree.treeViewState(handle);

  if (!state) {
    return;
  }

    if (state->scrollbarDragging) {
        state->scrollbarDragging = false;
        state->scrollbarDragOffsetY = 0.0f;
        tree.markPaintDirty(handle);
        return;
    }

  const TreeViewItemId pressed =
      state->pressed;

    const bool pressedChevron =
            state->pressedChevron;

    state->pressed = {};
    state->pressedChevron = false;

    if (!pressed) {
        return;
    }

    const Rect content =
            detail::visualContentRect(
                    tree,
                    handle);

    const TreeViewItemId released =
            detail::treeViewItemAt(
                    *state,
                    content,
                    mousePosition);

    if (released != pressed) {
        return;
    }

    TreeViewItemSlot* item =
            treeViewItem(
                    *state,
                    pressed);

    if (!item) {
        return;
    }

    if (pressedChevron) {
        if (!item->firstChild) {
            return;
        }

        item->expanded =
                !item->expanded;

        state->visibleItemsDirty =
                true;

        tree.markPaintDirty(
                handle);

        return;
    }

    setTreeViewSelectedItem(
            tree,
            events,
            handle,
            pressed);
}

} // namespace octo