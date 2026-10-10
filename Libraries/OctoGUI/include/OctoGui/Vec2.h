#pragma once

#include "OctoGui/Types.h"

#include <cmath>

namespace octogui {
// Vec2
//
// Simple 2D vector used for positions, sizes, offsets, padding, etc.
//
// We keep this as a plain trivially-copyable struct.
// GUI code usually wants simple value semantics, not heavy math classes.

struct Vec2 {
  f32 x = 0.0f;
  f32 y = 0.0f;

  constexpr Vec2() noexcept = default;

  constexpr Vec2(f32 xIn, f32 yIn) noexcept : x(xIn), y(yIn) {}

  // Common values

  [[nodiscard]]
  static constexpr Vec2 zero() noexcept {
    return Vec2{0.0f, 0.0f};
  }

  [[nodiscard]]
  static constexpr Vec2 one() noexcept {
    return Vec2{1.0f, 1.0f};
  }

  // Operations

  [[nodiscard]]
  constexpr f32 dot(Vec2 other) const noexcept {
    return x * other.x + y * other.y;
  }

  [[nodiscard]]
  constexpr f32 lengthSquared() const noexcept {
    return x * x + y * y;
  }

  [[nodiscard]]
  f32 length() const noexcept {
    return std::sqrt(lengthSquared());
  }

  [[nodiscard]]
  Vec2 normalized() const noexcept {
    const f32 len = length();

    if (len <= 0.0f) {
      return Vec2::zero();
    }

    return Vec2{x / len, y / len};
  }

  // Member operators

  constexpr Vec2 &operator+=(Vec2 rhs) noexcept {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  constexpr Vec2 &operator-=(Vec2 rhs) noexcept {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  constexpr Vec2 &operator*=(f32 rhs) noexcept {
    x *= rhs;
    y *= rhs;
    return *this;
  }

  constexpr Vec2 &operator/=(f32 rhs) noexcept {
    x /= rhs;
    y /= rhs;
    return *this;
  }

  friend constexpr bool operator==(Vec2 lhs, Vec2 rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
  }

  friend constexpr bool operator!=(Vec2 lhs, Vec2 rhs) noexcept {
    return !(lhs == rhs);
  }
};

// Free operators

[[nodiscard]]
constexpr Vec2 operator+(Vec2 lhs, Vec2 rhs) noexcept {
  return Vec2{lhs.x + rhs.x, lhs.y + rhs.y};
}

[[nodiscard]]
constexpr Vec2 operator-(Vec2 lhs, Vec2 rhs) noexcept {
  return Vec2{lhs.x - rhs.x, lhs.y - rhs.y};
}

[[nodiscard]]
constexpr Vec2 operator*(Vec2 lhs, f32 rhs) noexcept {
  return Vec2{lhs.x * rhs, lhs.y * rhs};
}

[[nodiscard]]
constexpr Vec2 operator*(f32 lhs, Vec2 rhs) noexcept {
  return rhs * lhs;
}

[[nodiscard]]
constexpr Vec2 operator/(Vec2 lhs, f32 rhs) noexcept {
  return Vec2{lhs.x / rhs, lhs.y / rhs};
}

[[nodiscard]]
constexpr Vec2 operator-(Vec2 v) noexcept {
  return Vec2{-v.x, -v.y};
}

// Convenience aliases
//
// For now, Size2 is just another Vec2.
//
// Later, if we want stronger type safety between positions and sizes, we can
// turn Size2 into its own struct without changing too much code.

using Size2 = Vec2;

} // namespace octogui