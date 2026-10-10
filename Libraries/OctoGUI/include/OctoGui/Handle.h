#pragma once

#include "OctoGui/Types.h"

namespace octogui {
//
// A generational handle used to safely reference objects stored in dense
// arrays.
//
// The actual resource is stored at `index`. The `generation` counter is used
// to detect stale handles after an object is removed and its slot is reused.
//
// Example:
//   struct NodeTag {};
//   using NodeHandle = Handle<NodeTag>;
//
template <typename Tag> struct Handle {
  u32 index = InvalidIndex;
  u32 generation = 0;

  constexpr Handle() noexcept = default;

  constexpr Handle(u32 indexIn, u32 generationIn) noexcept
      : index(indexIn), generation(generationIn) {}

  [[nodiscard]]
  constexpr bool isValid() const noexcept {
    return index != InvalidIndex;
  }

  constexpr void reset() noexcept {
    index = InvalidIndex;
    generation = 0;
  }

  [[nodiscard]]
  constexpr explicit operator bool() const noexcept {
    return isValid();
  }

  friend constexpr bool operator==(Handle lhs, Handle rhs) noexcept {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
  }

  friend constexpr bool operator!=(Handle lhs, Handle rhs) noexcept {
    return !(lhs == rhs);
  }
};

} // namespace octogui