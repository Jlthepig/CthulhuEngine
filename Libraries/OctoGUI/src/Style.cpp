#include "OctoGui/Style.h"

namespace octogui {
namespace {

void setState(VisualStyle &style, StyleState state, Color background,
              Color border, Color text,
              Color indicator = Color::transparent()) noexcept {
  StateStyle &s = stateStyle(style, state);
  s.background = background;
  s.border = border;
  s.text = text;
  s.indicator = indicator;
}

void applyControlFocusRing(VisualStyle &style, const Theme &theme) noexcept {
  style.focusRing.color = theme.focus.withAlpha(0.65f);
  style.focusRing.width = theme.focusRingWidth;
  style.focusRing.offset = theme.focusRingOffset;
}

} // namespace

Theme darkTheme() noexcept {
  Theme theme;

  theme.accent = Color::fromRGBA8(88, 139, 255);
  theme.accentHovered = Color::fromRGBA8(106, 153, 255);
  theme.accentActive = Color::fromRGBA8(70, 116, 231);
  theme.accentText = Color::white();
  theme.focus = theme.accent;

  theme.background = Color::fromRGBA8(14, 16, 20);
  theme.surface = Color::fromRGBA8(19, 22, 27);
  theme.surfaceRaised = Color::fromRGBA8(27, 31, 38);
  theme.surfaceHovered = Color::fromRGBA8(35, 40, 49);
  theme.surfaceActive = Color::fromRGBA8(22, 26, 32);
  theme.surfaceDisabled = Color::fromRGBA8(21, 24, 29);

  theme.border = Color::fromRGBA8(42, 48, 58);
  theme.borderStrong = Color::fromRGBA8(61, 69, 82);
  theme.borderDisabled = Color::fromRGBA8(34, 39, 47);

  theme.focusRingWidth = 2.0f;
  theme.focusRingOffset = 2.0f;

  theme.text = Color::fromRGBA8(235, 238, 244);
  theme.textMuted = Color::fromRGBA8(150, 160, 174);
  theme.textDisabled = Color::fromRGBA8(91, 99, 111);

  theme.panelRadius = 8.0f;
  theme.controlRadius = 6.0f;
  theme.smallRadius = 4.0f;
  theme.borderWidth = 1.0f;

  theme.elevationLow.color = Color::black().withAlpha(0.22f);
  theme.elevationLow.offset = Vec2{0.0f, 2.0f};
  theme.elevationLow.blur = 6.0f;

  theme.elevationMedium.color = Color::black().withAlpha(0.30f);
  theme.elevationMedium.offset = Vec2{0.0f, 4.0f};
  theme.elevationMedium.blur = 12.0f;

  theme.elevationHigh.color = Color::black().withAlpha(0.38f);
  theme.elevationHigh.offset = Vec2{0.0f, 7.0f};
  theme.elevationHigh.blur = 20.0f;

  return theme;
}

Theme lightTheme() noexcept {
  Theme theme;

  theme.accent = Color::fromRGBA8(59, 130, 246);
  theme.accentHovered = Color::fromRGBA8(37, 99, 235);
  theme.accentActive = Color::fromRGBA8(29, 78, 216);
  theme.accentText = Color::white();
  theme.focus = theme.accent;

  theme.background = Color::fromRGBA8(243, 245, 248);
  theme.surface = Color::fromRGBA8(255, 255, 255);
  theme.surfaceRaised = Color::fromRGBA8(248, 250, 252);
  theme.surfaceHovered = Color::fromRGBA8(238, 242, 247);
  theme.surfaceActive = Color::fromRGBA8(229, 234, 240);
  theme.surfaceDisabled = Color::fromRGBA8(244, 246, 248);

  theme.border = Color::fromRGBA8(214, 220, 228);
  theme.borderStrong = Color::fromRGBA8(181, 191, 204);
  theme.borderDisabled = Color::fromRGBA8(225, 229, 234);

  theme.focusRingWidth = 2.0f;
  theme.focusRingOffset = 2.0f;

  theme.text = Color::fromRGBA8(29, 33, 40);
  theme.textMuted = Color::fromRGBA8(101, 112, 127);
  theme.textDisabled = Color::fromRGBA8(154, 163, 175);

  theme.panelRadius = 8.0f;
  theme.controlRadius = 6.0f;
  theme.smallRadius = 4.0f;
  theme.borderWidth = 1.0f;

  theme.elevationLow.color = Color::black().withAlpha(0.10f);
  theme.elevationLow.offset = Vec2{0.0f, 2.0f};
  theme.elevationLow.blur = 6.0f;

  theme.elevationMedium.color = Color::black().withAlpha(0.16f);
  theme.elevationMedium.offset = Vec2{0.0f, 4.0f};
  theme.elevationMedium.blur = 12.0f;

  theme.elevationHigh.color = Color::black().withAlpha(0.22f);
  theme.elevationHigh.offset = Vec2{0.0f, 7.0f};
  theme.elevationHigh.blur = 20.0f;

  return theme;
}

Theme defaultTheme() noexcept { return darkTheme(); }

VisualStyle makeRootStyle(const Theme &theme) noexcept {
  VisualStyle style;

  setState(style, StyleState::Normal, theme.background, Color::transparent(),
           theme.text);
  setState(style, StyleState::Hovered, theme.background, Color::transparent(),
           theme.text);
  setState(style, StyleState::Active, theme.background, Color::transparent(),
           theme.text);
  setState(style, StyleState::Disabled, theme.background, Color::transparent(),
           theme.textDisabled);
  setState(style, StyleState::Focused, theme.background, Color::transparent(),
           theme.text);

  return style;
}

VisualStyle makePanelStyle(const Theme &theme) noexcept {
  VisualStyle style;

  style.cornerRadius = theme.panelRadius;
  style.borderWidth = theme.borderWidth;
  style.shadow = theme.elevationLow;

  setState(style, StyleState::Normal, theme.surface, theme.border, theme.text);
  setState(style, StyleState::Hovered, theme.surfaceHovered, theme.border,
           theme.text);
  setState(style, StyleState::Active, theme.surfaceActive, theme.borderStrong,
           theme.text);
  setState(style, StyleState::Disabled, theme.surfaceDisabled,
           theme.borderDisabled, theme.textDisabled);
  setState(style, StyleState::Focused, theme.surface, theme.focus, theme.text);

  return style;
}

VisualStyle makeButtonStyle(const Theme &theme) noexcept {
  VisualStyle style;

  style.cornerRadius = theme.controlRadius;
  style.borderWidth = theme.borderWidth;

  setState(style, StyleState::Normal, theme.surfaceRaised, theme.border,
           theme.text);
  setState(style, StyleState::Hovered, theme.surfaceHovered, theme.borderStrong,
           theme.text);
  setState(style, StyleState::Active, theme.surfaceActive, theme.accentActive,
           theme.text);
  setState(style, StyleState::Disabled, theme.surfaceDisabled,
           theme.borderDisabled, theme.textDisabled);
  setState(style, StyleState::Focused, theme.surfaceRaised, theme.focus,
           theme.text);

  stateStyle(style, StyleState::Normal).gradient = LinearGradient{
      theme.surfaceRaised, theme.surface, Vec2{0.0f, 1.0f}, true};
  stateStyle(style, StyleState::Hovered).gradient = LinearGradient{
      theme.surfaceHovered, theme.surfaceRaised, Vec2{0.0f, 1.0f}, true};
  stateStyle(style, StyleState::Active).gradient = LinearGradient{
      theme.surfaceRaised, theme.surfaceActive, Vec2{0.0f, 1.0f}, true};

  applyControlFocusRing(style, theme);
  style.pressedOffsetY = 1.0f;

  return style;
}

VisualStyle makeLabelStyle(const Theme &theme) noexcept {
  VisualStyle style;

  setState(style, StyleState::Normal, Color::transparent(),
           Color::transparent(), theme.text);
  setState(style, StyleState::Hovered, Color::transparent(),
           Color::transparent(), theme.text);
  setState(style, StyleState::Active, Color::transparent(),
           Color::transparent(), theme.text);
  setState(style, StyleState::Disabled, Color::transparent(),
           Color::transparent(), theme.textDisabled);
  setState(style, StyleState::Focused, Color::transparent(),
           Color::transparent(), theme.text);

  return style;
}

VisualStyle makeCheckboxStyle(const Theme &theme) noexcept {
  VisualStyle style;

  style.cornerRadius = theme.smallRadius;
  style.borderWidth = theme.borderWidth;

  setState(style, StyleState::Normal, theme.surfaceRaised, theme.borderStrong,
           theme.text, theme.accent);
  setState(style, StyleState::Hovered, theme.surfaceHovered, theme.focus,
           theme.text, theme.accentHovered);
  setState(style, StyleState::Active, theme.surfaceActive, theme.focus,
           theme.text, theme.accentActive);
  setState(style, StyleState::Disabled, theme.surfaceDisabled,
           theme.borderDisabled, theme.textDisabled, theme.textDisabled);
  setState(style, StyleState::Focused, theme.surfaceRaised, theme.focus,
           theme.text, theme.accent);

  applyControlFocusRing(style, theme);
  return style;
}

VisualStyle makeSliderStyle(const Theme &theme) noexcept {
  VisualStyle style;

  style.cornerRadius = theme.smallRadius;
  style.borderWidth = 0.0f;

  setState(style, StyleState::Normal, theme.surfaceRaised, theme.borderStrong,
           theme.text, theme.accent);
  setState(style, StyleState::Hovered, theme.surfaceHovered, theme.textMuted,
           theme.text, theme.accentHovered);
  setState(style, StyleState::Active, theme.surfaceActive, theme.text,
           theme.text, theme.accentActive);
  setState(style, StyleState::Disabled, theme.surfaceDisabled,
           theme.borderDisabled, theme.textDisabled, theme.textDisabled);
  setState(style, StyleState::Focused, theme.surfaceRaised, theme.focus,
           theme.text, theme.accent);

  applyControlFocusRing(style, theme);
  return style;
}

VisualStyle makeRadioButtonStyle(const Theme &theme) noexcept {
  VisualStyle style;

  style.borderWidth = theme.borderWidth;

  setState(style, StyleState::Normal, theme.surfaceRaised, theme.borderStrong,
           theme.text, theme.accent);
  setState(style, StyleState::Hovered, theme.surfaceHovered, theme.focus,
           theme.text, theme.accentHovered);
  setState(style, StyleState::Active, theme.surfaceActive, theme.focus,
           theme.text, theme.accentActive);
  setState(style, StyleState::Disabled, theme.surfaceDisabled,
           theme.borderDisabled, theme.textDisabled, theme.textDisabled);
  setState(style, StyleState::Focused, theme.surfaceRaised, theme.focus,
           theme.text, theme.accent);

  applyControlFocusRing(style, theme);
  return style;
}

VisualStyle makeTextInputStyle(const Theme &theme) noexcept {
  VisualStyle style;

  style.cornerRadius = theme.controlRadius;
  style.borderWidth = theme.borderWidth;

  setState(style, StyleState::Normal,
         theme.surfaceRaised, theme.border, theme.text, theme.accent);

  setState(style, StyleState::Hovered,
          theme.surfaceRaised, theme.borderStrong, theme.text, theme.accentHovered);

  setState(style, StyleState::Active,
          theme.surfaceRaised, theme.focus, theme.text, theme.accentActive);

  setState(style, StyleState::Disabled,
          theme.surfaceDisabled, theme.borderDisabled,
          theme.textDisabled, theme.textDisabled);

  setState(style, StyleState::Focused,
          theme.surfaceRaised, theme.focus, theme.text, theme.accent);

  applyControlFocusRing(style, theme);

  return style;
}

VisualStyle makeSeparatorStyle(const Theme& theme) noexcept {
  VisualStyle style;

  style.borderWidth = 1.0f;

  setState(style, StyleState::Normal,
           Color::transparent(), Color::transparent(),
           theme.textMuted, theme.borderStrong);

  setState(style, StyleState::Hovered,
           Color::transparent(), Color::transparent(),
           theme.textMuted, theme.borderStrong);

  setState(style, StyleState::Active,
           Color::transparent(), Color::transparent(),
           theme.textMuted, theme.borderStrong);

  setState(style, StyleState::Disabled,
           Color::transparent(), Color::transparent(),
           theme.textDisabled, theme.borderDisabled);

  setState(style, StyleState::Focused,
           Color::transparent(), Color::transparent(),
           theme.textMuted, theme.borderStrong);

  return style;
}

} // namespace octogui
