#include "editorTheme.hpp"

namespace Cthulhu::Editor
{
octogui::Theme cthulhuTheme() noexcept
{
    using octogui::Color;

    octogui::Theme theme = octogui::darkTheme();

    theme.accent = Color::fromRGBA8(20, 128, 111);
    theme.accentHovered = Color::fromRGBA8(26, 142, 123);
    theme.accentActive = Color::fromRGBA8(16, 110, 95);
    theme.accentText = Color::white();
    theme.focus = Color::fromRGBA8(48, 184, 160);

    theme.background = Color::fromRGBA8(10, 12, 12);
    theme.surface = Color::fromRGBA8(23, 26, 26);
    theme.surfaceRaised = Color::fromRGBA8(31, 36, 36);
    theme.surfaceHovered = Color::fromRGBA8(41, 47, 47);
    theme.surfaceActive = Color::fromRGBA8(26, 30, 30);
    theme.surfaceDisabled = Color::fromRGBA8(25, 28, 28);

    theme.border = Color::fromRGBA8(46, 53, 53);
    theme.borderStrong = Color::fromRGBA8(70, 79, 78);
    theme.borderDisabled = Color::fromRGBA8(36, 41, 41);

    theme.text = Color::fromRGBA8(228, 234, 232);
    theme.textMuted = Color::fromRGBA8(148, 160, 157);
    theme.textDisabled = Color::fromRGBA8(92, 102, 100);

    theme.panelRadius = 4.0f;
    theme.controlRadius = 3.0f;
    theme.smallRadius = 2.0f;

    theme.elevationLow = octogui::ShadowStyle{};

    return theme;
}
} // namespace Cthulhu::Editor