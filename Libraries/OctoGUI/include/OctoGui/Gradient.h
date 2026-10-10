#pragma once

#include "OctoGui/Color.h"
#include "OctoGui/Vec2.h"

namespace octogui {

struct LinearGradient {
  Color start = Color::transparent();
  Color end = Color::transparent();
  Vec2 direction{0.0f, 1.0f};
  bool enabled = false;

  friend constexpr bool operator==(const LinearGradient &,
                                   const LinearGradient &) noexcept = default;
};

} // namespace octogui