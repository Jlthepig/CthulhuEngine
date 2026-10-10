#pragma once

#include "OctoGui/Tree.h"

namespace octogui {

namespace detail {

struct StyleTarget {
  VisualStyle *direct = nullptr;
  Tree *tree = nullptr;
  NodeHandle handle{};

  [[nodiscard]]
  VisualStyle *style() noexcept {
    if (direct) {
      return direct;
    }

    if (tree && tree->isValid(handle)) {
      return tree->visualStyle(handle);
    }

    return nullptr;
  }

  [[nodiscard]]
  const VisualStyle *style() const noexcept {
    if (direct) {
      return direct;
    }

    if (tree && tree->isValid(handle)) {
      return static_cast<const Tree &>(*tree).visualStyle(handle);
    }

    return nullptr;
  }

  void dirty() noexcept {
    if (tree) {
      tree->markStyleDirty(handle);
    }
  }
};

struct StyleFloatProperty {
  StyleTarget target;
  f32 VisualStyle::*field;

  StyleFloatProperty &operator=(f32 value) noexcept {
    if (VisualStyle *style = target.style()) {
      if (style->*field != value) {
        style->*field = value;
        target.dirty();
      }
    }
    return *this;
  }

  StyleFloatProperty &operator=(const StyleFloatProperty &other) noexcept {
    return *this = static_cast<f32>(other);
  }

  operator f32() const noexcept {
    if (const VisualStyle *style = target.style()) {
      return style->*field;
    }
    return 0.0f;
  }
};

struct StyleColorProperty {
  StyleTarget target;
  StyleState state;
  Color StateStyle::*field;

  StyleColorProperty &operator=(const Color &value) noexcept {
    if (VisualStyle *style = target.style()) {
      StateStyle &s = stateStyle(*style, state);
      Color &slot = s.*field;

      bool changed = false;

      if (!(slot == value)) {
        slot = value;
        changed = true;
      }

      if (field == &StateStyle::background && s.gradient.enabled) {
        s.gradient.enabled = false;
        changed = true;
      }

      if (changed) {
        target.dirty();
      }
    }
    return *this;
  }

  StyleColorProperty &operator=(const StyleColorProperty &other) noexcept {
    return *this = static_cast<Color>(other);
  }

  operator Color() const noexcept {
    if (const VisualStyle *style = target.style()) {
      return stateStyle(*style, state).*field;
    }

    if (field == &StateStyle::text) {
      return Color::white();
    }

    return Color::transparent();
  }
};

struct StyleAllColorProperty {
  StyleTarget target;
  Color StateStyle::*field;

  StyleAllColorProperty &operator=(Color value) noexcept {
    if (VisualStyle *style = target.style()) {
      bool changed = false;

      for (usize i = 0; i < StyleStateCount; ++i) {
        StateStyle &s = stateStyle(*style, static_cast<StyleState>(i));
        Color &slot = s.*field;

        if (!(slot == value)) {
          slot = value;
          changed = true;
        }

        if (field == &StateStyle::background && s.gradient.enabled) {
          s.gradient.enabled = false;
          changed = true;
        }
      }

      if (changed) {
        target.dirty();
      }
    }
    return *this;
  }

  StyleAllColorProperty &operator=(
      const StyleAllColorProperty &other) noexcept {
    if (VisualStyle *destination = target.style()) {
      if (const VisualStyle *source = other.target.style()) {
        bool changed = false;

        for (usize i = 0; i < StyleStateCount; ++i) {
          StateStyle &destinationState =
              stateStyle(*destination, static_cast<StyleState>(i));
          const StateStyle &sourceState =
              stateStyle(*source, static_cast<StyleState>(i));
          Color &destinationSlot = destinationState.*field;
          const Color &sourceSlot = sourceState.*other.field;

          if (!(destinationSlot == sourceSlot)) {
            destinationSlot = sourceSlot;
            changed = true;
          }

          if (field == &StateStyle::background &&
              destinationState.gradient.enabled) {
            destinationState.gradient.enabled = false;
            changed = true;
          }
        }

        if (changed) {
          target.dirty();
        }
      }
    }
    return *this;
  }
};

struct StyleShadowProperty {
  StyleTarget target;

  StyleShadowProperty &operator=(const ShadowStyle &value) noexcept {
    if (VisualStyle *style = target.style()) {
      if (!(style->shadow == value)) {
        style->shadow = value;
        target.dirty();
      }
    }
    return *this;
  }

  StyleShadowProperty &operator=(const StyleShadowProperty &other) noexcept {
    return *this = static_cast<ShadowStyle>(other);
  }

  operator ShadowStyle() const noexcept {
    if (const VisualStyle *style = target.style()) {
      return style->shadow;
    }
    return ShadowStyle{};
  }
};

struct StyleFocusRingProperty {
  StyleTarget target;

  StyleFocusRingProperty &operator=(const FocusRingStyle &value) noexcept {
    if (VisualStyle *style = target.style()) {
      if (!(style->focusRing == value)) {
        style->focusRing = value;
        target.dirty();
      }
    }

    return *this;
  }

  StyleFocusRingProperty &operator=(
      const StyleFocusRingProperty &other) noexcept {
    return *this = static_cast<FocusRingStyle>(other);
  }

  operator FocusRingStyle() const noexcept {
    if (const VisualStyle *style = target.style()) {
      return style->focusRing;
    }
    return FocusRingStyle{};
  }
};

struct StyleAccentEdgeProperty {
  StyleTarget target;

  StyleAccentEdgeProperty &operator=(const AccentEdgeStyle &value) noexcept {
    if (VisualStyle *style = target.style()) {
      if (!(style->accentEdge == value)) {
        style->accentEdge = value;
        target.dirty();
      }
    }

    return *this;
  }

  StyleAccentEdgeProperty &operator=(
      const StyleAccentEdgeProperty &other) noexcept {
    return *this = static_cast<AccentEdgeStyle>(other);
  }

  operator AccentEdgeStyle() const noexcept {
    if (const VisualStyle *style = target.style()) {
      return style->accentEdge;
    }
    return AccentEdgeStyle{};
  }
};

struct StyleGradientProperty {
  StyleTarget target;
  StyleState state;

  StyleGradientProperty &operator=(const LinearGradient &value) noexcept {
    if (VisualStyle *style = target.style()) {
      LinearGradient &slot = stateStyle(*style, state).gradient;

      if (!(slot == value)) {
        slot = value;
        target.dirty();
      }
    }

    return *this;
  }

  StyleGradientProperty &operator=(
      const StyleGradientProperty &other) noexcept {
    return *this = static_cast<LinearGradient>(other);
  }

  operator LinearGradient() const noexcept {
    if (const VisualStyle *style = target.style()) {
      return stateStyle(*style, state).gradient;
    }

    return LinearGradient{};
  }
};

struct StyleAllGradientProperty {
  StyleTarget target;

  StyleAllGradientProperty &operator=(const LinearGradient &value) noexcept {
    if (VisualStyle *style = target.style()) {
      bool changed = false;

      for (usize i = 0; i < StyleStateCount; ++i) {
        LinearGradient &slot =
            stateStyle(*style, static_cast<StyleState>(i)).gradient;

        if (!(slot == value)) {
          slot = value;
          changed = true;
        }
      }

      if (changed) {
        target.dirty();
      }
    }
    return *this;
  }

  StyleAllGradientProperty &operator=(
      const StyleAllGradientProperty &other) noexcept {
    if (VisualStyle *destination = target.style()) {
      if (const VisualStyle *source = other.target.style()) {
        bool changed = false;

        for (usize i = 0; i < StyleStateCount; ++i) {
          LinearGradient &destinationGradient =
              stateStyle(*destination, static_cast<StyleState>(i)).gradient;
          const LinearGradient &sourceGradient =
              stateStyle(*source, static_cast<StyleState>(i)).gradient;

          if (!(destinationGradient == sourceGradient)) {
            destinationGradient = sourceGradient;
            changed = true;
          }
        }

        if (changed) {
          target.dirty();
        }
      }
    }
    return *this;
  }
};

struct StyleStateProxy {
  StyleColorProperty background;
  StyleGradientProperty gradient;
  StyleColorProperty border;
  StyleColorProperty text;
  StyleColorProperty indicator;

  StyleStateProxy(StyleTarget target, StyleState state) noexcept
      : background{target, state, &StateStyle::background},
        gradient{target, state}, border{target, state, &StateStyle::border},
        text{target, state, &StateStyle::text},
        indicator{target, state, &StateStyle::indicator} {}
};
struct StyleAllProxy {
  StyleAllColorProperty background;
  StyleAllGradientProperty gradient;
  StyleAllColorProperty border;
  StyleAllColorProperty text;
  StyleAllColorProperty indicator;

  StyleAllProxy(StyleTarget target) noexcept
      : background{target, &StateStyle::background}, gradient{target},
        border{target, &StateStyle::border}, text{target, &StateStyle::text},
        indicator{target, &StateStyle::indicator} {}
};
} // namespace detail

class StyleProxy {
public:
  detail::StyleFloatProperty cornerRadius;
  detail::StyleFloatProperty borderWidth;
  detail::StyleFloatProperty pressedOffsetY;

  detail::StyleShadowProperty shadow;
  detail::StyleFocusRingProperty focusRing;
  detail::StyleAccentEdgeProperty accentEdge;

  detail::StyleStateProxy normal;
  detail::StyleStateProxy hovered;
  detail::StyleStateProxy active;
  detail::StyleStateProxy disabled;
  detail::StyleStateProxy focused;

  detail::StyleAllProxy all;

  StyleProxy(detail::StyleTarget target) noexcept
      : cornerRadius{target, &VisualStyle::cornerRadius},
        borderWidth{target, &VisualStyle::borderWidth},
        pressedOffsetY{target, &VisualStyle::pressedOffsetY}, shadow{target},
        focusRing{target}, accentEdge{target},
        normal{target, StyleState::Normal}, hovered{target, StyleState::Hovered},
        active{target, StyleState::Active},
        disabled{target, StyleState::Disabled},
        focused{target, StyleState::Focused}, all{target} {}
};

} // namespace octogui