#pragma once

#include <string>

#include "OctoGui/Types.h"

namespace octogui {

enum class TextAlign : u8 { Start, Center, End };

struct TextState {
  std::string text;

  TextAlign horizontalAlign = TextAlign::Center;
  TextAlign verticalAlign = TextAlign::Center;
};

} // namespace octogui