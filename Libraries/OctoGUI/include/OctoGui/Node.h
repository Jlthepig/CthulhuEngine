#pragma once

#include "OctoGui/Handle.h"
#include "OctoGui/Types.h"

namespace octogui {
struct NodeTag {};

using NodeHandle = Handle<NodeTag>;

enum class NodeType : u8 {
  Root,
  Container,
  Button,
  Label,
  Checkbox,
  Slider,
  RadioButton,
  TextInput,
  NumericInput,
  Separator,
  SplitContainer,
  ComboBox,
  CollapsibleSection,
  ProgressBar,
  TreeView,
  Icon,
  Custom
};

enum class NodeFlags : u32 {
  None = 0,
  Visible = 1u << 0,
  Enabled = 1u << 1,
  Interactive = 1u << 2,
  Focusable = 1u << 3,
  LayoutDirty = 1u << 4,
  PaintDirty = 1u << 5,
  ChildrenDirty = 1u << 6,
  Checked = 1u << 7,
  PaintChildrenDirty = 1u << 8,
  StyleDirty = 1u << 9
};

enum class SeparatorOrientation : u8 {
  Horizontal,
  Vertical
};

enum class SplitOrientation : u8 {
  Horizontal,
  Vertical
};

enum class NumericInputType : u8 {
  Integer,
  Float
};

inline constexpr u32 InvalidTreeViewItemIndex = 0xFFFFFFFFu;

struct TreeViewItemId {
  u32 index = InvalidTreeViewItemIndex;
  u32 generation = 0;

  [[nodiscard]]
  constexpr explicit operator bool() const noexcept {
    return index != InvalidTreeViewItemIndex;
  }

  friend constexpr bool operator==(
      TreeViewItemId lhs,
      TreeViewItemId rhs) noexcept {
    return lhs.index == rhs.index &&
           lhs.generation == rhs.generation;
  }

  friend constexpr bool operator!=(
      TreeViewItemId lhs,
      TreeViewItemId rhs) noexcept {
    return !(lhs == rhs);
  }
};

constexpr NodeFlags operator|(NodeFlags lhs, NodeFlags rhs) noexcept {
  return static_cast<NodeFlags>(static_cast<u32>(lhs) | static_cast<u32>(rhs));
}

constexpr NodeFlags operator&(NodeFlags lhs, NodeFlags rhs) noexcept {
  return static_cast<NodeFlags>(static_cast<u32>(lhs) & static_cast<u32>(rhs));
}

constexpr NodeFlags &operator|=(NodeFlags &lhs, NodeFlags rhs) noexcept {
  lhs = lhs | rhs;
  return lhs;
}

constexpr NodeFlags &operator&=(NodeFlags &lhs, NodeFlags rhs) noexcept {
  lhs = lhs & rhs;
  return lhs;
}

[[nodiscard]]
constexpr bool hasFlag(NodeFlags flags, NodeFlags flag) noexcept {
  return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0u;
}

[[nodiscard]]
constexpr NodeFlags addFlags(NodeFlags flags, NodeFlags flagsToAdd) noexcept {
  return flags | flagsToAdd;
}

[[nodiscard]]
constexpr NodeFlags removeFlags(NodeFlags flags,
                                NodeFlags flagsToRemove) noexcept {
  return static_cast<NodeFlags>(static_cast<u32>(flags) &
                                ~static_cast<u32>(flagsToRemove));
}
} // namespace octogui