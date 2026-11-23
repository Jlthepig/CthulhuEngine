#pragma once

namespace Cthulhu::Editor
{
// Shared editor sizes. Use these instead of literals so every panel follows
// the same spacing scale and control heights.

// Spacing scale for gaps and padding.
inline constexpr float SpaceXs = 2.0f;
inline constexpr float SpaceS = 4.0f;
inline constexpr float SpaceM = 8.0f;
inline constexpr float SpaceL = 12.0f;
inline constexpr float SpaceXl = 16.0f;

// Heights.
inline constexpr float RowHeight = 24.0f;          // single-line labels, breadcrumbs, list rows
inline constexpr float ControlHeight = 26.0f;      // buttons, text and number inputs, dropdowns
inline constexpr float ToolbarHeight = 32.0f;      // panel and viewport toolbars
inline constexpr float PanelHeaderHeight = 28.0f;  // panel titles, scene tabs, settings sections
inline constexpr float DialogButtonHeight = 30.0f; // dialog and settings action buttons
inline constexpr float StatusBarHeight = 22.0f;
inline constexpr float TopBarHeight = 36.0f;

// Icons: one glyph size, shown in a 24 px square or a 28 px-wide toolbar button.
inline constexpr float IconSize = 16.0f;
inline constexpr float IconButtonSize = 24.0f;
inline constexpr float ToolButtonWidth = 28.0f;
inline constexpr float LogoSize = 24.0f;

// OctoGUI draws tree rows at a fixed 26 px (detail::TreeViewRowHeight). The
// Hierarchy rename box must line up with them, so keep this in step.
inline constexpr float TreeRowHeight = 26.0f;
} // namespace Cthulhu::Editor
