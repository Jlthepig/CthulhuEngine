#pragma once

#include "OctoGui/Style.h"

namespace Cthulhu::Editor
{

[[nodiscard]] octogui::Theme cthulhuTheme() noexcept;

// red for errors and rejected drops.
inline constexpr octogui::Color DangerColor = octogui::Color::fromRGBA8(232, 98, 98);
inline constexpr octogui::Color LiveColor = octogui::Color::fromRGBA8(121, 253, 0);
inline constexpr octogui::Color WarningColor = octogui::Color::fromRGBA8(230, 193, 80);

inline constexpr octogui::ShadowStyle PanelShadow{octogui::Color::black().withAlpha(0.85f), octogui::Vec2{0.0f, 2.0f}, 6.0f, 0.0f};
inline constexpr octogui::ShadowStyle TopBarShadow{octogui::Color::black().withAlpha(0.80f), octogui::Vec2{0.0f, 2.0f}, 10.0f, 0.0f};

} // namespace Cthulhu::Editor