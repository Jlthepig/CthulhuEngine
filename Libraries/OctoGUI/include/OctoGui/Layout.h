#pragma once

#include "OctoGui/Node.h"
#include "OctoGui/Rect.h"
#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {
inline constexpr f32 InfiniteLayoutSize = 1.0e30f;

enum class LayoutMode : u8 { Manual, Vertical, Horizontal, Stack };

enum class Alignment : u8 { Start, Center, End, Stretch };

struct Padding {
  f32 left = 0.0f;
  f32 top = 0.0f;
  f32 right = 0.0f;
  f32 bottom = 0.0f;

  constexpr Padding() noexcept = default;

  constexpr Padding(f32 all) noexcept
      : left(all), top(all), right(all), bottom(all) {}

  constexpr Padding(f32 horizontal, f32 vertical) noexcept
      : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}

  constexpr Padding(f32 leftIn, f32 topIn, f32 rightIn, f32 bottomIn) noexcept
      : left(leftIn), top(topIn), right(rightIn), bottom(bottomIn) {}

  [[nodiscard]]
  constexpr f32 horizontal() const noexcept {
    return left + right;
  }

  [[nodiscard]]
  constexpr f32 vertical() const noexcept {
    return top + bottom;
  }

  friend constexpr bool operator==(const Padding &lhs,
                                   const Padding &rhs) noexcept {
    return lhs.left == rhs.left && lhs.top == rhs.top &&
           lhs.right == rhs.right && lhs.bottom == rhs.bottom;
  }

  friend constexpr bool operator!=(const Padding &lhs,
                                   const Padding &rhs) noexcept {
    return !(lhs == rhs);
  }
};

struct LayoutStyle {
  LayoutMode mode = LayoutMode::Vertical;

  Alignment horizontalAlignment = Alignment::Stretch;
  Alignment verticalAlignment = Alignment::Start;

  f32 gap = 0.0f;

  Padding padding{};

  Vec2 minSize{};
  Vec2 preferredSize{};
  Vec2 maxSize{InfiniteLayoutSize, InfiniteLayoutSize};

  bool fitContentY = false;
  bool clip = false;
  bool scrollY = false;

  [[nodiscard]]
  constexpr bool isManual() const noexcept {
    return mode == LayoutMode::Manual;
  }

  [[nodiscard]]
  constexpr bool isVertical() const noexcept {
    return mode == LayoutMode::Vertical;
  }

  [[nodiscard]]
  constexpr bool isHorizontal() const noexcept {
    return mode == LayoutMode::Horizontal;
  }

  [[nodiscard]]
  constexpr bool isStack() const noexcept {
    return mode == LayoutMode::Stack;
  }
};

struct LayoutInput {
  Vec2 availableSize{};
  f32 dpiScale = 1.0f;
};

struct LayoutResult {
  Rect rect = Rect::zero();
  Vec2 contentSize{};
  bool clipped = false;
  bool overflowed = false;
};

class Tree;

void solveLayout(Tree &tree, NodeHandle root, const LayoutInput &input);
} // namespace octogui