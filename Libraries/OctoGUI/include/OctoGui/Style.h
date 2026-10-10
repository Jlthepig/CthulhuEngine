#pragma once

#include <array>

#include "OctoGui/Color.h"
#include "OctoGui/Gradient.h"
#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {
enum class StyleState : u8 {
  Normal = 0,
  Hovered,
  Active,
  Disabled,
  Focused,
  Count
};

inline constexpr usize StyleStateCount = static_cast<usize>(StyleState::Count);
struct StateStyle {
  Color background = Color::transparent();
  LinearGradient gradient{};
  Color border = Color::transparent();
  Color text = Color::white();
  Color indicator = Color::transparent();

  friend constexpr bool operator==(const StateStyle &,
                                   const StateStyle &) noexcept = default;
};

struct ShadowStyle {
  Color color = Color::transparent();
  Vec2 offset{};
  f32 blur = 0.0f;
  f32 spread = 0.0f;

  friend constexpr bool operator==(const ShadowStyle &,
                                   const ShadowStyle &) noexcept = default;
};

struct FocusRingStyle {
  Color color = Color::transparent();
  f32 width = 0.0f;
  f32 offset = 0.0f;

  friend constexpr bool operator==(const FocusRingStyle &,
                                   const FocusRingStyle &) noexcept = default;
};

enum class AccentEdgeSide : u8 { Left, Right, Top, Bottom };

struct AccentEdgeStyle {
  Color color = Color::transparent();
  f32 width = 0.0f;
  f32 inset = 0.0f;
  AccentEdgeSide side = AccentEdgeSide::Left;

  friend constexpr bool operator==(const AccentEdgeStyle &,
                                   const AccentEdgeStyle &) noexcept = default;
};
struct VisualStyle {
  f32 cornerRadius = 0.0f;
  f32 borderWidth = 0.0f;
  f32 pressedOffsetY = 0.0f;

  FocusRingStyle focusRing{};
  ShadowStyle shadow{};
  AccentEdgeStyle accentEdge{};

  std::array<StateStyle, StyleStateCount> states{};

  friend constexpr bool operator==(const VisualStyle &,
                                   const VisualStyle &) noexcept = default;
};

struct Theme {
  Color accent = Color::white();
  Color accentHovered = Color::white();
  Color accentActive = Color::white();
  Color accentText = Color::white();
  Color focus = Color::white();

  Color background = Color::black();
  Color surface = Color::black();
  Color surfaceRaised = Color::black();
  Color surfaceHovered = Color::black();
  Color surfaceActive = Color::black();
  Color surfaceDisabled = Color::black();

  Color border = Color::black();
  Color borderStrong = Color::black();
  Color borderDisabled = Color::black();

  f32 focusRingWidth = 2.0f;
  f32 focusRingOffset = 2.0f;

  Color text = Color::white();
  Color textMuted = Color::white();
  Color textDisabled = Color::white();

  f32 panelRadius = 8.0f;
  f32 controlRadius = 6.0f;
  f32 smallRadius = 4.0f;
  f32 borderWidth = 1.0f;

  ShadowStyle elevationLow{};
  ShadowStyle elevationMedium{};
  ShadowStyle elevationHigh{};
};

[[nodiscard]]
constexpr usize styleStateIndex(StyleState state) noexcept {
  const usize index = static_cast<usize>(state);
  return index < StyleStateCount ? index : usize(0);
}

[[nodiscard]]
constexpr StateStyle &stateStyle(VisualStyle &style,
                                 StyleState state) noexcept {
  return style.states[styleStateIndex(state)];
}

[[nodiscard]]
constexpr const StateStyle &stateStyle(const VisualStyle &style,
                                       StyleState state) noexcept {
  return style.states[styleStateIndex(state)];
}

Theme darkTheme() noexcept;
Theme lightTheme() noexcept;
Theme defaultTheme() noexcept;

VisualStyle makeRootStyle(const Theme &theme) noexcept;
VisualStyle makePanelStyle(const Theme &theme) noexcept;

VisualStyle makeButtonStyle(const Theme &theme) noexcept;

VisualStyle makeLabelStyle(const Theme &theme) noexcept;

VisualStyle makeCheckboxStyle(const Theme &theme) noexcept;

VisualStyle makeSliderStyle(const Theme &theme) noexcept;

VisualStyle makeRadioButtonStyle(const Theme &theme) noexcept;

VisualStyle makeTextInputStyle(const Theme &theme) noexcept;

VisualStyle makeSeparatorStyle(const Theme& theme) noexcept;

} // namespace octogui