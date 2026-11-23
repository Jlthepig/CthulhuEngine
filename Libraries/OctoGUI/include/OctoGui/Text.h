#pragma once

#include <string>

#include "OctoGui/Types.h"

namespace octogui {

enum class TextAlign : u8 { Start, Center, End };

enum class FontSlot : u8 { Default, Secondary };

inline constexpr usize FontSlotCount = 2;

struct TextState {
  std::string text;

  TextAlign horizontalAlign = TextAlign::Center;
  TextAlign verticalAlign = TextAlign::Center;
  FontSlot font = FontSlot::Default;
};

} // namespace octogui