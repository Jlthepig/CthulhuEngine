#pragma once

#include "OctoGui/Style.h"

namespace Cthulhu::Editor
{

[[nodiscard]] octogui::Theme cthulhuTheme() noexcept;

// red for errors and rejected drops.
inline constexpr octogui::Color DangerColor = octogui::Color::fromRGBA8(232, 98, 98);
inline constexpr octogui::Color LiveColor = octogui::Color::fromRGBA8(121, 253, 0);

} // namespace Cthulhu::Editor