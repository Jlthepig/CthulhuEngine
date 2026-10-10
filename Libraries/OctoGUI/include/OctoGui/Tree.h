#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "OctoGui/Layout.h"
#include "OctoGui/Node.h"
#include "OctoGui/Rect.h"
#include "OctoGui/Style.h"
#include "OctoGui/Text.h"
#include "OctoGui/Types.h"

namespace octogui {

struct SliderState {
  f32 value = 0.0f;
  f32 minValue = 0.0f;
  f32 maxValue = 0.0f;
  f32 step = 0.0f;
};

inline constexpr u32 DefaultTextInputCharacterLimit = 2048;
inline constexpr u32 DefaultNumericInputCharacterLimit = 24;
struct TextInputState {
  u32 cursor = 0;
  u32 selectionAnchor = 0;

  u32 maxCharacters = DefaultTextInputCharacterLimit;

  f32 scrollX = 0.0f;
  f32 caretBlinkTimer = 0.0f;

  bool draggingSelection = false;
  bool readOnly = false;

  std::string placeholder;
};

struct SeparatorState {
    SeparatorOrientation orientation = SeparatorOrientation::Horizontal;
};
struct SplitContainerState {
  NodeHandle firstPane{};
  NodeHandle secondPane{};

  SplitOrientation orientation =
      SplitOrientation::Horizontal;

  f32 ratio = 0.5f;

  f32 minFirst = 80.0f;
  f32 minSecond = 80.0f;
};
struct NumericInputState {
  f64 value = 0.0;
  f64 minValue = -100000.0f;
  f64 maxValue = 1000000.0f;
  f64 step = 0.1;

  f64 dragStartValue = 0.0;
  f32 dragStartX = 0.0f;

  bool dragCandidate = false;
  bool dragging = false;

  NumericInputType type = NumericInputType::Float;
};
struct ComboBoxState {
  std::vector<std::string> items;

  i32 selectedIndex = -1;
  i32 hoveredIndex = -1;

  f32 popupScrollY = 0.0f;

  bool open = false;
};

struct CollapsibleSectionState {
  NodeHandle content{};
};

struct ProgressBarState {
  f64 value = 0.0;
  f64 minValue = 0.0;
  f64 maxValue = 1.0;

  bool showPercentage = true;
};

struct TreeViewItemSlot {
  std::string label;

  u64 userValue = 0;

  TreeViewItemId parent{};
  TreeViewItemId firstChild{};
  TreeViewItemId lastChild{};

  TreeViewItemId previousSibling{};
  TreeViewItemId nextSibling{};

  u32 generation = 1;

  bool alive = false;
  bool expanded = true;
};

struct TreeViewVisibleItem {
  TreeViewItemId item{};
  u32 depth = 0;
};

struct TreeViewState {
  std::vector<TreeViewItemSlot> items;
  std::vector<u32> freeItems;

  std::vector<TreeViewVisibleItem> visibleItems;

  TreeViewItemId firstRoot{};
  TreeViewItemId lastRoot{};

  TreeViewItemId selected{};
  TreeViewItemId hovered{};
  TreeViewItemId pressed{};

  bool pressedChevron = false;

  u32 itemCount = 0;

  f32 scrollY = 0.0f;

  f32 scrollbarDragOffsetY = 0.0f;

  bool scrollbarHovered = false;
  bool scrollbarDragging = false;
  bool scrollToSelectionPending = false;

  bool visibleItemsDirty = true;
};
struct IconState {
  std::string name;
  Color color = Color::white();
};
class Tree {
  struct StyleHandle {
    u32 index = InvalidIndex;

    [[nodiscard]]
    constexpr bool isValid() const noexcept {
      return index != InvalidIndex;
    }
  };

public:
  Tree() = default;

  Tree(const Tree&) = delete;
  Tree& operator=(const Tree&) = delete;

  Tree(Tree&&) = default;
  Tree& operator=(Tree&&) = default;

  [[nodiscard]]
  NodeHandle create(NodeType type = NodeType::Container,
                    NodeHandle parent = {});

  [[nodiscard]]
  NodeHandle createRoot(NodeType type = NodeType::Root);

  void destroy(NodeHandle handle);

  void setParent(NodeHandle handle, NodeHandle newParent);

  [[nodiscard]]
  bool isValid(NodeHandle handle) const noexcept {
    return handle.index < static_cast<u32>(alive.size()) &&
           alive[handle.index] != 0 &&
           generations[handle.index] == handle.generation;
  }

  [[nodiscard]]
  bool isDescendant(NodeHandle ancestor, NodeHandle descendant) const noexcept;

  [[nodiscard]]
  usize size() const noexcept {
    return static_cast<usize>(aliveCount);
  }

  [[nodiscard]]
  bool empty() const noexcept {
    return aliveCount == 0;
  }

  [[nodiscard]]
  usize capacity() const noexcept {
    return generations.size();
  }

  void reserve(usize capacity);

  void clear() noexcept;

  [[nodiscard]]
  NodeType type(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeType::Custom;
    }

    return types[handle.index];
  }

  [[nodiscard]]
  NodeFlags flags(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeFlags::None;
    }

    return nodeFlags[handle.index];
  }

  void setInteractive(NodeHandle handle, bool interactive) {
    if (!isValid(handle)) {
      return;
    }

    if (interactive) {
      nodeFlags[handle.index] = addFlags(nodeFlags[handle.index], NodeFlags::Interactive);
    } else {
      nodeFlags[handle.index] = removeFlags(nodeFlags[handle.index], NodeFlags::Interactive);
    }
  }

  void setFocusable(NodeHandle handle, bool focusable) {
    if (!isValid(handle)) {
      return;
    }

    if (focusable) {
      nodeFlags[handle.index] = addFlags(nodeFlags[handle.index], NodeFlags::Focusable);
    } else {
      nodeFlags[handle.index] = removeFlags(nodeFlags[handle.index], NodeFlags::Focusable);
    }
  }

  void setChecked(NodeHandle handle, bool checked) {
    if (!isValid(handle)) {
      return;
    }

    if (checked) {
      nodeFlags[handle.index] = addFlags(nodeFlags[handle.index], NodeFlags::Checked);
    } else {
      nodeFlags[handle.index] = removeFlags(nodeFlags[handle.index], NodeFlags::Checked);
    }
  }

  [[nodiscard]]
  i32 zLayer(NodeHandle handle) const noexcept;

  void setZLayer(NodeHandle handle, i32 zLayer);

  [[nodiscard]]
  bool isVisible(NodeHandle handle) const noexcept {
    return isValid(handle) && hasFlag(nodeFlags[handle.index], NodeFlags::Visible);
  }

  void setVisible(NodeHandle handle, bool visible) {
    if (!isValid(handle)) {
      return;
    }

    const bool wasVisible = hasFlag(nodeFlags[handle.index], NodeFlags::Visible);

    if (wasVisible == visible) {
      return;
    }

    if (visible) {
      nodeFlags[handle.index] = addFlags(nodeFlags[handle.index], NodeFlags::Visible);
    } else {
        nodeFlags[handle.index] =
          removeFlags(nodeFlags[handle.index], NodeFlags::Visible);
    }

    markLayoutDirty(handle);
    markPaintDirty(handle);

    const NodeHandle parentHandle = parents[handle.index];

    if (isValid(parentHandle)) {
      markLayoutDirty(parentHandle);
    }
  }

  [[nodiscard]]
  bool isEnabled(NodeHandle handle) const noexcept {
    return isValid(handle) && hasFlag(nodeFlags[handle.index], NodeFlags::Enabled);
  }

  void setEnabled(NodeHandle handle, bool enabled) {
    if (!isValid(handle)) {
      return;
    }

    const bool wasEnabled = hasFlag(nodeFlags[handle.index], NodeFlags::Enabled);

    if (wasEnabled == enabled) {
      return;
    }

    if (enabled) {
      nodeFlags[handle.index] = addFlags(nodeFlags[handle.index], NodeFlags::Enabled);
    } else {
        nodeFlags[handle.index] =
          removeFlags(nodeFlags[handle.index], NodeFlags::Enabled);
    }

    markPaintDirty(handle);
  }

  [[nodiscard]]
  NodeHandle parent(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeHandle{};
    }

    return parents[handle.index];
  }

  [[nodiscard]]
  NodeHandle firstChild(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeHandle{};
    }

    return firstChildren[handle.index];
  }

  [[nodiscard]]
  NodeHandle lastChild(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeHandle{};
    }

    return lastChildren[handle.index];
  }

  [[nodiscard]]
  NodeHandle nextSibling(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeHandle{};
    }

    return nextSiblings[handle.index];
  }

  [[nodiscard]]
  NodeHandle previousSibling(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return NodeHandle{};
    }

    return prevSiblings[handle.index];
  }

  [[nodiscard]]
  u32 childCount(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return 0;
    }

    return childCounts[handle.index];
  }

  [[nodiscard]]
  Rect rect(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return Rect::zero();
    }

    return rects[handle.index];
  }

  void setRect(NodeHandle handle, Rect value) {
    if (!isValid(handle)) {
      return;
    }

    rects[handle.index] = value;
  }

  [[nodiscard]]
  f32 scrollOffsetY(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return 0.0f;
    }
    return scrollOffsetsY[handle.index];
  }

  [[nodiscard]]
  f32 maxScrollOffsetY(NodeHandle handle) const noexcept;

  bool setScrollOffsetY(NodeHandle handle, f32 offset) noexcept;

  [[nodiscard]]
  TextState *textState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 textIndex = textStateIndices[handle.index];

    if (textIndex == InvalidIndex) {
      return nullptr;
    }

    return &textStates[textIndex];
  }

  [[nodiscard]]
  const TextState *textState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 textIndex = textStateIndices[handle.index];

    if (textIndex == InvalidIndex) {
      return nullptr;
    }

    return &textStates[textIndex];
  }

  [[nodiscard]]
  std::string_view text(NodeHandle handle) const noexcept {
    const TextState *state = textState(handle);

    if (!state) {
      return {};
    }

    return state->text;
  }

  void setText(NodeHandle handle, std::string_view value) {
    if (!isValid(handle)) {
      return;
    }

    TextState *state = textState(handle);

    if (!state) {
      if (value.empty()) {
        return;
      }

      state = &ensureTextState(handle.index);
    }

    if (state->text == value) {
      return;
    }

    state->text.assign(value);

    if (TextInputState *inputState = textInputState(handle)) {
      const u32 textSize = static_cast<u32>(state->text.size());

      if (inputState->cursor > textSize) {
        inputState->cursor = textSize;
      }

      if (inputState->selectionAnchor > textSize) {
        inputState->selectionAnchor = textSize;
      }

      inputState->scrollX = 0.0f;
      inputState->caretBlinkTimer = 0.0f;
    }

    const NodeType nodeType = type(handle);

    if (nodeType != NodeType::TextInput &&
        nodeType != NodeType::NumericInput) {
      markLayoutDirty(handle);
}

    markPaintDirty(handle);
  }

  void setTextAlign(NodeHandle handle, TextAlign horizontal,
                    TextAlign vertical) {
    if (!isValid(handle)) {
      return;
    }

    TextState *state = textState(handle);

    if (!state) {
      if (horizontal == TextAlign::Center && vertical == TextAlign::Center) {
        return;
      }

      state = &ensureTextState(handle.index);
    }

    if (state->horizontalAlign == horizontal &&
        state->verticalAlign == vertical) {
      return;
    }

    state->horizontalAlign = horizontal;
    state->verticalAlign = vertical;

    markPaintDirty(handle);
  }

  [[nodiscard]]
  LayoutStyle *layoutStyle(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    return &layoutStyles[handle.index];
  }

  [[nodiscard]]
  const LayoutStyle *layoutStyle(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    return &layoutStyles[handle.index];
  }

  void setLayoutStyle(NodeHandle handle, const LayoutStyle &style) {
    if (!isValid(handle)) {
      return;
    }

    layoutStyles[handle.index] = style;
    markLayoutDirty(handle);
  }

  [[nodiscard]]
  LayoutResult *layoutResult(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    return &layoutResults[handle.index];
  }

  [[nodiscard]]
  const LayoutResult *layoutResult(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    return &layoutResults[handle.index];
  }

  void markLayoutDirty(NodeHandle handle) noexcept;
  void clearLayoutDirty(NodeHandle handle) noexcept;

  [[nodiscard]]
  bool isLayoutDirty(NodeHandle handle) const noexcept {
    return isValid(handle) &&
           hasFlag(nodeFlags[handle.index], NodeFlags::LayoutDirty);
  }

  [[nodiscard]]
  bool hasDirtyLayoutChildren(NodeHandle handle) const noexcept {
    return isValid(handle) &&
           hasFlag(nodeFlags[handle.index], NodeFlags::ChildrenDirty);
  }

  void markStyleDirty(NodeHandle handle) noexcept;
  void markStyleDirtySubtree(NodeHandle handle);
  void clearStyleDirty(NodeHandle handle) noexcept;

  [[nodiscard]]
  bool isStyleDirty(NodeHandle handle) const noexcept {
    return isValid(handle) &&
           hasFlag(nodeFlags[handle.index], NodeFlags::StyleDirty);
  }

  [[nodiscard]]
  VisualStyle *visualStyle(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    ensureUniqueStyle(handle);

    const StyleHandle styleHandle = styleHandles[handle.index];

    if (!styleHandle.isValid()) {
      return nullptr;
    }

    return &styleTable[styleHandle.index].style;
  }

  [[nodiscard]]
  const VisualStyle *visualStyle(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const StyleHandle styleHandle = styleHandles[handle.index];

    if (!styleHandle.isValid() || styleHandle.index >= styleTable.size()) {
      return nullptr;
    }

    return &styleTable[styleHandle.index].style;
  }

  void setVisualStyle(NodeHandle handle, const VisualStyle &style) {
    if (!isValid(handle)) {
      return;
    }

    const VisualStyle *current =
        static_cast<const Tree &>(*this).visualStyle(handle);

    if (current && *current == style) {
      return;
    }

    const StyleHandle newHandle = acquireStyle(style);
    const StyleHandle oldHandle = styleHandles[handle.index];

    styleHandles[handle.index] = newHandle;
    releaseStyle(oldHandle);

    markStyleDirty(handle);
  }

  void markPaintDirty(NodeHandle handle) noexcept;
  void clearPaintDirty(NodeHandle handle) noexcept;

  [[nodiscard]]
  bool isPaintDirty(NodeHandle handle) const noexcept {
    return isValid(handle) &&
           hasFlag(nodeFlags[handle.index], NodeFlags::PaintDirty);
  }

  [[nodiscard]]
  bool hasDirtyPaintChildren(NodeHandle handle) const noexcept {
    return isValid(handle) &&
           hasFlag(nodeFlags[handle.index], NodeFlags::PaintChildrenDirty);
  }

  [[nodiscard]]
  SliderState *sliderState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 sliderIndex = sliderStateIndices[handle.index];

    if (sliderIndex == InvalidIndex) {
      return nullptr;
    }

    return &sliderStates[sliderIndex];
  }

  [[nodiscard]]
  const SliderState *sliderState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 sliderIndex = sliderStateIndices[handle.index];

    if (sliderIndex == InvalidIndex) {
      return nullptr;
    }

    return &sliderStates[sliderIndex];
  }

  [[nodiscard]]
  TextInputState *textInputState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = textInputStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &textInputStates[stateIndex];
  }

  [[nodiscard]]
  const TextInputState *textInputState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = textInputStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &textInputStates[stateIndex];
  }

  SeparatorState* separatorState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {return nullptr;}

    const u32 stateIndex =
        separatorStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &separatorStates[stateIndex];
  }

  const SeparatorState* separatorState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {return nullptr;}

    const u32 stateIndex =
        separatorStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &separatorStates[stateIndex];
  }

  [[nodiscard]]
  SplitContainerState* splitContainerState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = splitContainerStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &splitContainerStates[stateIndex];
  }

  [[nodiscard]]
  const SplitContainerState* splitContainerState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = splitContainerStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &splitContainerStates[stateIndex];
  }

  NumericInputState* numericInputState(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return nullptr;
  }

  const u32 index = numericInputStateIndices[handle.index];

  if (index == InvalidIndex) {
    return nullptr;
  }

  return &numericInputStates[index];
}

const NumericInputState* numericInputState(NodeHandle handle) const noexcept {
  if (!isValid(handle)) {
    return nullptr;
  }

  const u32 index = numericInputStateIndices[handle.index];

  if (index == InvalidIndex) {
    return nullptr;
  }

  return &numericInputStates[index];
}

  [[nodiscard]]
  ComboBoxState *comboBoxState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = comboBoxStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &comboBoxStates[stateIndex];
  }

  [[nodiscard]]
  const ComboBoxState *comboBoxState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = comboBoxStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &comboBoxStates[stateIndex];
  }

  [[nodiscard]]
  CollapsibleSectionState *collapsibleSectionState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = collapsibleSectionStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &collapsibleSectionStates[stateIndex];
  }

  [[nodiscard]]
  const CollapsibleSectionState *collapsibleSectionState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = collapsibleSectionStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &collapsibleSectionStates[stateIndex];
  }  

  [[nodiscard]]
  ProgressBarState *progressBarState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = progressBarStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &progressBarStates[stateIndex];
  }

  [[nodiscard]]
  const ProgressBarState *progressBarState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = progressBarStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &progressBarStates[stateIndex];
  }

  [[nodiscard]]
  TreeViewState *treeViewState(NodeHandle handle) noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = treeViewStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &treeViewStates[stateIndex];
  }

  [[nodiscard]]
  const TreeViewState *treeViewState(NodeHandle handle) const noexcept {
    if (!isValid(handle)) {
      return nullptr;
    }

    const u32 stateIndex = treeViewStateIndices[handle.index];

    if (stateIndex == InvalidIndex) {
      return nullptr;
    }

    return &treeViewStates[stateIndex];
  }

  IconState* iconState(NodeHandle handle) noexcept {
  if (!isValid(handle)) {return nullptr;}

  const u32 index =
      iconStateIndices[handle.index];

  if (index == InvalidIndex) {return nullptr;}

  return &iconStates[index];
}

const IconState* iconState(NodeHandle handle) const noexcept {
  if (!isValid(handle)) {return nullptr;}

  const u32 index =
      iconStateIndices[handle.index];

  if (index == InvalidIndex) {return nullptr;}

  return &iconStates[index];
}

private:
  [[nodiscard]]
  u32 allocateSlot(NodeType type);
  void initializeSlot(u32 index, NodeType type);
  void freeSlot(u32 index);
  void linkChild(NodeHandle child, NodeHandle parent);
  void unlink(NodeHandle handle);

  [[nodiscard]]
  StyleHandle acquireStyle(
      const VisualStyle &style); // searches for an identical shared style first

  [[nodiscard]]
  StyleHandle createUniqueStyle(
      const VisualStyle &style); // will always create a separate entry and is
                                 // used for copy on write

  void releaseStyle(StyleHandle handle) noexcept;
  void ensureUniqueStyle(NodeHandle handle);

  TextState &ensureTextState(u32 nodeIndex);
  void removeTextState(u32 nodeIndex) noexcept;

  std::vector<u32> generations;
  std::vector<u8> alive;

  std::vector<NodeType> types;
  std::vector<NodeFlags> nodeFlags;

  std::vector<NodeHandle> parents;
  std::vector<NodeHandle> firstChildren;
  std::vector<NodeHandle> lastChildren;
  std::vector<NodeHandle> prevSiblings;
  std::vector<NodeHandle> nextSiblings;

  std::vector<u32> childCounts;
  std::vector<i32> zLayers;

  std::vector<Rect> rects;
  std::vector<f32> scrollOffsetsY;
  std::vector<TextState> textStates;

  std::vector<u32> textOwners;
  std::vector<u32> textStateIndices;
  std::vector<SliderState> sliderStates;
  std::vector<u32> sliderOwners;
  std::vector<u32> sliderStateIndices;
  std::vector<TextInputState> textInputStates;
  std::vector<u32> textInputOwners;
  std::vector<u32> textInputStateIndices;
  std::vector<SeparatorState> separatorStates;
  std::vector<u32> separatorOwners;
  std::vector<u32> separatorStateIndices;
  std::vector<SplitContainerState> splitContainerStates;
  std::vector<u32> splitContainerOwners;
  std::vector<u32> splitContainerStateIndices;
  std::vector<NumericInputState> numericInputStates;
  std::vector<u32> numericInputOwners;
  std::vector<u32> numericInputStateIndices;
  std::vector<ComboBoxState> comboBoxStates;
  std::vector<u32> comboBoxOwners;
  std::vector<u32> comboBoxStateIndices;
  std::vector<CollapsibleSectionState> collapsibleSectionStates;
  std::vector<u32> collapsibleSectionOwners;
  std::vector<u32> collapsibleSectionStateIndices;
  std::vector<ProgressBarState> progressBarStates;
  std::vector<u32> progressBarOwners;
  std::vector<u32> progressBarStateIndices;
  std::vector<TreeViewState> treeViewStates;
  std::vector<u32> treeViewOwners;
  std::vector<u32> treeViewStateIndices;
  std::vector<IconState> iconStates;
  std::vector<u32> iconOwners;
  std::vector<u32> iconStateIndices;


  std::vector<LayoutStyle> layoutStyles;
  std::vector<LayoutResult> layoutResults;

  struct StyleEntry {
    VisualStyle style{};
    u32 refCount = 0;
  };

  std::vector<StyleHandle> styleHandles;
  std::vector<StyleEntry> styleTable;
  std::vector<u32> freeStyleSlots;

  std::vector<u32> freeList;

  u32 aliveCount = 0;
};
} // namespace octogui