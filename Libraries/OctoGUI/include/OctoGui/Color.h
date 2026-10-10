#pragma once

#include "OctoGui/Types.h"

namespace octogui {
// ColorU32
//
// Packed 8-bit RGBA color
//
// R in bits 24.. 31
// G in bits 16..23
// B in bits 8..15
// A in bits 0..7
//
// useful for draw lists vertex data and backend submission

using ColorU32 = u32;

namespace detail {
  
[[nodiscard]]
constexpr f32 clamp01(f32 value) noexcept {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

[[nodiscard]]
constexpr u8 colorFloatToByte(f32 value) noexcept {
  return static_cast<u8>(clamp01(value) * 255.0f + 0.5f);
}

[[nodiscard]]
constexpr f32 colorByteToFloat(u8 value) noexcept {
  return static_cast<f32>((value) / 255.0f);
}
} // namespace detail

// Colour
//
// RGBA color stored as floating point values in the range [0, 1]
//
// This is the main editing format fpr OctoGUI
//
// Note - Colors are straight alpha by default Use premultipliedAlpha() if the
// backend expects it
//      - Color space is left backend defined on purpose for now

struct Color {
  f32 r = 0.0f;
  f32 g = 0.0f;
  f32 b = 0.0f;
  f32 a = 1.0f;

  constexpr Color() noexcept = default;

  constexpr Color(f32 rIn, f32 gIn, f32 bIn, f32 aIn = 1.0) noexcept
      : r(rIn), g(gIn), b(bIn), a(aIn) {}

  // Constructors

  [[nodiscard]]
  static constexpr Color fromRGBA(f32 rIn, f32 gIn, f32 bIn,
                                  f32 aIn = 1.0f) noexcept {
    return Color{rIn, gIn, bIn, aIn};
  }

  [[nodiscard]]
  static constexpr Color fromRGBA8(u8 rIn, u8 gIn, u8 bIn,
                                   u8 aIn = 255) noexcept {
    return Color{
        detail::colorByteToFloat(rIn),
        detail::colorByteToFloat(gIn),
        detail::colorByteToFloat(bIn),
        detail::colorByteToFloat(aIn),
    };
  }

  [[nodiscard]]
  static constexpr Color unpackRGBA8(ColorU32 packed) noexcept {
    return fromRGBA8(static_cast<u8>((packed >> 24) & 0xFFu),
                     static_cast<u8>((packed >> 16) & 0xFFu),
                     static_cast<u8>((packed >> 8) & 0xFFu),
                     static_cast<u8>((packed >> 0) & 0xFFu));
  }

  // Conversion
  [[nodiscard]]
  constexpr ColorU32 packRGBA8() const noexcept {
    const u32 red = static_cast<u32>(detail::colorFloatToByte(r));
    const u32 green = static_cast<u32>(detail::colorFloatToByte(g));
    const u32 blue = static_cast<u32>(detail::colorFloatToByte(b));
    const u32 alpha = static_cast<u32>(detail::colorFloatToByte(a));

    return (red << 24) | (green << 16) | (blue << 8) | alpha;
  }

  [[nodiscard]]
  constexpr Color withAlpha(f32 newAlpha) const noexcept {
    return Color{r, g, b, newAlpha};
  }

  [[nodiscard]]
  constexpr Color premultipliedAlpha() const noexcept {
    return Color{
        r * a,
        g * a,
        b * a,
        a,
    };
  }

  [[nodiscard]]
  constexpr bool isOpaque() const noexcept {
    return a >= 1.0f;
  }

  [[nodiscard]]
  constexpr bool isTransparent() const noexcept {
    return a <= 0.0f;
  }

  // Operators
  friend constexpr bool operator==(const Color &lhs,
                                   const Color &rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
  }

  friend constexpr bool operator!=(const Color &lhs,
                                   const Color &rhs) noexcept {
    return !(lhs == rhs);
  }

  // Common Colours

  [[nodiscard]]
  static constexpr Color transparent() noexcept {
    return Color{0.0f, 0.0f, 0.0f, 0.0f};
  }

  [[nodiscard]]
  static constexpr Color black() noexcept {
    return Color{0.0f, 0.0f, 0.0f, 1.0f};
  }

  [[nodiscard]]
  static constexpr Color white() noexcept {
    return Color{1.0f, 1.0f, 1.0f, 1.0f};
  }

  [[nodiscard]]
  static constexpr Color red() noexcept {
    return Color{1.0f, 0.0f, 0.0f, 1.0f};
  }

  [[nodiscard]]
  static constexpr Color green() noexcept {
    return Color{0.0f, 1.0f, 0.0f, 1.0f};
  }

  [[nodiscard]]
  static constexpr Color blue() noexcept {
    return Color{0.0f, 0.0f, 1.0f, 1.0f};
  }
};
} // namespace octogui