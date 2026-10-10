#pragma once

#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {
// axis aligned rectangle defined by a position and a size
//
// used a ton for layout bounds clipping regions and hit testing

struct Rect {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 w = 0.0f;
  f32 h = 0.0f;

  constexpr Rect() noexcept = default;

  constexpr Rect(f32 xIn, f32 yIn, f32 wIn, f32 hIn) noexcept
      : x(xIn), y(yIn), w(wIn), h(hIn) {}

  constexpr Rect(Vec2 pos, Vec2 size) noexcept
      : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

  // common values

  [[nodiscard]]
  static constexpr Rect zero() noexcept {
    return Rect{
        0.0f,
        0.0f,
        0.0f,
        0.0f,
    };
  }

  // accessors

  [[nodiscard]]
  constexpr Vec2 position() const noexcept {
    return Vec2{x, y};
  }

  [[nodiscard]]
  constexpr Vec2 size() const noexcept {
    return Vec2{w, h};
  }

  [[nodiscard]]
  constexpr f32 minX() const noexcept {
    return x;
  }

  [[nodiscard]]
  constexpr f32 minY() const noexcept {
    return y;
  }

  [[nodiscard]]
  constexpr f32 maxX() const noexcept {
    return x + w;
  }

  [[nodiscard]]
  constexpr f32 maxY() const noexcept {
    return y + h;
  }

  [[nodiscard]]
  constexpr Vec2 min() const noexcept {
    return Vec2{x, y};
  }

  [[nodiscard]]
  constexpr Vec2 max() const noexcept {
    return Vec2{x + w, y + h};
  }

  [[nodiscard]]
  constexpr Vec2 center() const noexcept {
    return Vec2{x + w * 0.5f, y + h * 0.5f};
  }

  [[nodiscard]]
  constexpr f32 area() const noexcept {
    return w * h;
  }

  [[nodiscard]]
  constexpr bool isEmpty() const noexcept {
    return w <= 0.0f || h <= 0.0f;
  }

  // hit testing & spatial queries

  [[nodiscard]]
  constexpr bool contains(Vec2 point) const noexcept {
    return point.x >= x && point.x < x + w && point.y >= y && point.y < y + h;
  }

  [[nodiscard]]
  constexpr bool contains(Rect other) const noexcept {
    return other.x >= x && other.maxX() <= maxX() && other.y >= y &&
           other.maxY() <= maxY();
  }

  [[nodiscard]]
  constexpr bool intersects(Rect other) const noexcept {
    return x < other.maxX() && maxX() > other.x && y < other.maxY() &&
           maxY() > other.x;
  }

  // modifications

  // returns new rect expanded outward by given amount on all sides
  [[nodiscard]]
  constexpr Rect expand(Vec2 amount) const noexcept {
    return Rect{x - amount.x, y - amount.y, w + amount.x * 2.0f,
                h + amount.y * 2.0f};
  }

  // returns a new rect shrunk inward by amount on every side
  [[nodiscard]]
  constexpr Rect shrink(f32 amount) const noexcept {
    return Rect{x + amount, y + amount, w - amount * 2.0f, h - amount * 2.0f};
  }

  [[nodiscard]]
  constexpr Rect shrink(Vec2 amount) const noexcept {
    return Rect{x + amount.x, y + amount.y, w - amount.x * 2.0f,
                h - amount.y * 2.0f};
  }

  [[nodiscard]]
  constexpr Rect translate(Vec2 offset) const noexcept {
    return Rect{x + offset.x, y + offset.y, w, h};
  }

  [[nodiscard]]
  constexpr Rect intersection(Rect other) const noexcept {
    f32 ix = x > other.x ? x : other.x;
    f32 iy = y > other.y ? y : other.y;

    f32 ix2 = maxX() < other.maxX() ? maxX() : other.maxX();
    f32 iy2 = maxY() < other.maxY() ? maxY() : other.maxY();

    f32 iw = ix2 - ix;
    f32 ih = iy2 - iy;

    if (iw <= 0.0f || ih <= 0.0f) {
      return Rect::zero();
    }

    return Rect{ix, iy, iw, ih};
  }

  // Returns the smallest rectangle that contains both rectangles
  [[nodiscard]]
  constexpr Rect boundingBox(Rect other) const noexcept {
    if (isEmpty())
      return other;
    if (other.isEmpty())
      return *this;

    f32 bx = x < other.x ? x : other.x;
    f32 by = y < other.y ? y : other.y;

    f32 bx2 = maxX() > other.maxX() ? maxX() : other.maxX();
    f32 by2 = maxY() > other.maxY() ? maxY() : other.maxY();

    return Rect{bx, by, bx2 - bx, by2 - by};
  }

  friend constexpr bool operator==(const Rect &lhs, const Rect &rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.w == rhs.w && lhs.h == rhs.h;
  }

  friend constexpr bool operator!=(const Rect &lhs, const Rect &rhs) noexcept {
    return !(lhs == rhs);
  }
};
} // namespace octogui