#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace octogui {
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;

enum class IconColorMode : u8 {
  Tintable,
  Original
};

// Constants

inline constexpr u32 InvalidIndex = std::numeric_limits<u32>::max();

} // namespace octogui