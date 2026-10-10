#pragma once

#include "OctoGui/DrawList.h"
#include "OctoGui/Font.h"
#include "OctoGui/Tree.h"

#include "Systems/InteractionSystem.h"

namespace octogui::detail {

inline StyleState resolveStyleState(
    const Tree& tree,
    NodeHandle node,
    const InteractionState& interaction) noexcept {
  if (!tree.isEnabled(node)) {return StyleState::Disabled;}
  if (node == interaction.active) {return StyleState::Active;}

  if (node == interaction.hovered &&
      hasFlag(tree.flags(node), NodeFlags::Interactive)) {
    return StyleState::Hovered;
  }

  if (node == interaction.focused) {return StyleState::Focused;}

  return StyleState::Normal;
}

inline void addFocusRing(
    DrawList& list,
    Rect rect,
    const VisualStyle& style,
    StyleState state) {
  if (state != StyleState::Focused) {return;}

  if (style.focusRing.width <= 0.0f ||
      style.focusRing.color.isTransparent()) {
    return;
  }

  const f32 expansion =
      style.focusRing.offset +
      style.focusRing.width * 0.5f;

  const Rect ring{
      rect.x - expansion,
      rect.y - expansion,
      rect.w + expansion * 2.0f,
      rect.h + expansion * 2.0f};

  const f32 radius =
      style.cornerRadius + expansion;

  if (radius > 0.0f) {
    list.addRoundedRectOutline(
        ring,
        style.focusRing.color,
        radius,
        style.focusRing.width);
  } else {
    list.addRectOutline(
        ring,
        style.focusRing.color,
        style.focusRing.width);
  }
}

inline f32 fontTextHeight(const FontMetrics& metrics) noexcept {
  const f32 ascent =
      metrics.ascent > 0.0f
          ? metrics.ascent
          : 0.0f;

  const f32 descent =
      metrics.descent < 0.0f
          ? metrics.descent
          : -metrics.descent;

  const f32 height = ascent - descent;

  return height > 0.0f
      ? height
      : metrics.lineHeight;
}

inline f32 centeredTextTop(Rect rect, f32 textHeight) noexcept {
  if (rect.h < textHeight) {return rect.y;}

  return rect.y +
         (rect.h - textHeight) * 0.5f;
}

} // namespace octogui::detail