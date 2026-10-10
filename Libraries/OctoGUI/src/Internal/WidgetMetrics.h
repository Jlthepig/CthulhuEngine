#pragma once

#include "OctoGui/Layout.h"
#include "OctoGui/Rect.h"
#include "OctoGui/Types.h"
namespace octogui::detail {

inline constexpr f32 SliderTrackHeight = 4.0f;
inline constexpr f32 SliderHandleRadius = 7.0f;
inline constexpr f32 SliderMinHandleRadius = 4.0f;

inline constexpr f32 CheckboxBoxSize = 16.0f;
inline constexpr f32 CheckboxSpacing = 7.0f;

inline constexpr f32 RadioButtonBoxSize = 16.0f;
inline constexpr f32 RadioButtonSpacing = 7.0f;

inline constexpr f32 ScrollbarWidth = 8.0f;
inline constexpr f32 ScrollbarInset = 3.0f;
inline constexpr f32 ScrollbarMinThumbHeight = 28.0f;
inline constexpr f32 ScrollbarPageFactor = 0.9f;

inline constexpr f32 TextInputCaretScrollMargin = 3.0f;
inline constexpr f32 TextInputDragScrollSpeed = 600.0f;

inline constexpr f32 TextInputCaretWidth = 1.5f;

inline constexpr f32 TextInputCaretBlinkPeriod = 1.0f;
inline constexpr f32 TextInputCaretVisibleDuration = 0.5f;

inline constexpr f32 TextInputSelectionAlpha = 0.30f;
inline constexpr f32 TextInputPlaceholderAlpha = 0.55f;

inline constexpr f32 NumericInputDragThreshold = 4.0f;
inline constexpr f32 NumericInputDragPixelsPerStep = 4.0f;

inline constexpr f32 ComboBoxChevronSize = 8.0f;
inline constexpr f32 ComboBoxChevronThickness = 1.5f;
inline constexpr f32 ComboBoxChevronGap = 10.0f;

inline constexpr f32 ComboBoxPopupGap = 4.0f;
inline constexpr f32 ComboBoxPopupPadding = 4.0f;
inline constexpr f32 ComboBoxItemHeight = 30.0f;
inline constexpr f32 ComboBoxPopupCornerRadius = 6.0f;

inline constexpr usize ComboBoxMaxVisibleItems = 8;
inline constexpr f32 ComboBoxPopupViewportMargin = 6.0f;
inline constexpr f32 ComboBoxWheelItems = 3.0f;

inline constexpr f32 ComboBoxScrollbarWidth = 4.0f;
inline constexpr f32 ComboBoxScrollbarInset = 2.0f;
inline constexpr f32 ComboBoxScrollbarMinThumbHeight = 20.0f;

inline constexpr f32 CollapsibleSectionHeight = 34.0f;
inline constexpr f32 CollapsibleSectionMinHeight = 28.0f;

inline constexpr f32 CollapsibleSectionChevronSize = 7.0f;
inline constexpr f32 CollapsibleSectionChevronThickness = 1.5f;
inline constexpr f32 CollapsibleSectionChevronInset = 10.0f;
inline constexpr f32 CollapsibleSectionTextInset = 28.0f;

inline constexpr f32 ProgressBarHeight = 24.0f;
inline constexpr f32 ProgressBarMinHeight = 14.0f;
inline constexpr f32 ProgressBarFillInset = 2.0f;

inline constexpr f32 TreeViewRowHeight = 26.0f;
inline constexpr f32 TreeViewIndentWidth = 16.0f;

inline constexpr f32 TreeViewChevronAreaWidth = 16.0f;
inline constexpr f32 TreeViewChevronSize = 7.0f;
inline constexpr f32 TreeViewChevronThickness = 1.5f;

inline constexpr f32 TreeViewTextGap = 4.0f;

inline constexpr f32 TreeViewSelectionAccentWidth = 2.0f;
inline constexpr f32 TreeViewRowCornerRadius = 3.0f;

inline constexpr f32 IconDefaultSize = 32.0f;

inline constexpr f32 SplitDividerWidth = 4.0f;
inline constexpr f32 SplitDividerHitPadding = 4.0f;
inline constexpr f32 SplitDefaultMinPaneSize = 80.0f;

struct VerticalScrollbarGeometry {
  Rect track{};
  Rect thumb{};
  bool visible = false;
};

[[nodiscard]]
inline VerticalScrollbarGeometry
verticalScrollbarGeometry(Rect viewport, f32 contentHeight, f32 scrollOffset,
                          f32 maxScrollOffset) noexcept {
  VerticalScrollbarGeometry geometry{};

  if (viewport.isEmpty() || contentHeight <= viewport.h ||
      maxScrollOffset <= 0.0f) {
    return geometry;
  }

  f32 availableWidth = viewport.w - ScrollbarInset * 2.0f;

  if (availableWidth <= 0.0f) {
    return geometry;
  }

  f32 trackWidth = ScrollbarWidth;

  if (trackWidth > availableWidth) {
    trackWidth = availableWidth;
  }

  const f32 trackHeight = viewport.h - ScrollbarInset * 2.0f;

  if (trackHeight <= 0.0f) {
    return geometry;
  }

  geometry.track = Rect{viewport.maxX() - ScrollbarInset - trackWidth,
                        viewport.y + ScrollbarInset, trackWidth, trackHeight};

  f32 thumbHeight = trackHeight * (viewport.h / contentHeight);

  if (thumbHeight < ScrollbarMinThumbHeight) {
    thumbHeight = ScrollbarMinThumbHeight;
  }

  if (thumbHeight > trackHeight) {
    thumbHeight = trackHeight;
  }

  f32 t = scrollOffset / maxScrollOffset;

  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }

  const f32 travel = trackHeight - thumbHeight;

  geometry.thumb = Rect{geometry.track.x, geometry.track.y + travel * t,
                        geometry.track.w, thumbHeight};

  geometry.visible = true;

  return geometry;
}

inline Rect verticalScrollbarViewport(
    Rect rect,
    const Padding& padding) noexcept {
  f32 height =
      rect.h - padding.vertical();

  if (height < 0.0f) {
    height = 0.0f;
  }

  return Rect{
      rect.x,
      rect.y + padding.top,
      rect.w,
      height};
}

[[nodiscard]]
inline f32 sliderHandleRadiusForContent(Rect content) noexcept {
  f32 radius = SliderHandleRadius;
  const f32 maxRadius = content.h * 0.5f;

  if (radius > maxRadius) {
    radius = maxRadius;
  }
  if (radius < SliderMinHandleRadius && maxRadius >= SliderMinHandleRadius) {
    radius = SliderMinHandleRadius;
  }
  if (radius < 0.0f) {
    radius = 0.0f;
  }

  return radius;
}

inline Rect comboBoxPopupRect(
    Rect anchor,
    Rect viewport,
    usize itemCount) noexcept {
  if (itemCount == 0 || viewport.isEmpty()) {
    return Rect::zero();
  }

  const f32 margin =
      ComboBoxPopupViewportMargin;

  Rect safeViewport{
      viewport.x + margin,
      viewport.y + margin,
      viewport.w - margin * 2.0f,
      viewport.h - margin * 2.0f};

  if (safeViewport.isEmpty()) {
    return Rect::zero();
  }

  const usize visibleItems =
      itemCount < ComboBoxMaxVisibleItems
          ? itemCount
          : ComboBoxMaxVisibleItems;

  const f32 desiredHeight =
      ComboBoxPopupPadding * 2.0f +
      static_cast<f32>(visibleItems) *
          ComboBoxItemHeight;

  const f32 belowY =
      anchor.maxY() + ComboBoxPopupGap;

  const f32 aboveBottom =
      anchor.y - ComboBoxPopupGap;

  f32 spaceBelow =
      safeViewport.maxY() - belowY;

  f32 spaceAbove =
      aboveBottom - safeViewport.y;

  if (spaceBelow < 0.0f) {
    spaceBelow = 0.0f;
  }

  if (spaceAbove < 0.0f) {
    spaceAbove = 0.0f;
  }

  const bool placeBelow =
      desiredHeight <= spaceBelow ||
      spaceBelow >= spaceAbove;

  const f32 availableHeight =
      placeBelow
          ? spaceBelow
          : spaceAbove;

  f32 height =
      desiredHeight < availableHeight
          ? desiredHeight
          : availableHeight;

  if (height <= ComboBoxPopupPadding * 2.0f) {
    return Rect::zero();
  }

  f32 width = anchor.w;

  if (width > safeViewport.w) {
    width = safeViewport.w;
  }

  f32 x = anchor.x;

  if (x < safeViewport.x) {
    x = safeViewport.x;
  }

  if (x + width > safeViewport.maxX()) {
    x = safeViewport.maxX() - width;
  }

  const f32 y =
      placeBelow
          ? belowY
          : aboveBottom - height;

  return Rect{x, y, width, height};
}

inline Rect comboBoxPopupContentRect(
    Rect popup) noexcept {
  f32 width =
      popup.w - ComboBoxPopupPadding * 2.0f;

  f32 height =
      popup.h - ComboBoxPopupPadding * 2.0f;

  if (width < 0.0f) {
    width = 0.0f;
  }

  if (height < 0.0f) {
    height = 0.0f;
  }

  return Rect{
      popup.x + ComboBoxPopupPadding,
      popup.y + ComboBoxPopupPadding,
      width,
      height};
}

inline Rect comboBoxItemRect(
    Rect popup,
    usize index,
    f32 scrollY) noexcept {
  const Rect content =
      comboBoxPopupContentRect(popup);

  return Rect{
      content.x,
      content.y +
          static_cast<f32>(index) *
              ComboBoxItemHeight -
          scrollY,
      content.w,
      ComboBoxItemHeight};
}

inline f32 comboBoxMaxScrollY(
    Rect popup,
    usize itemCount) noexcept {
  const Rect content =
      comboBoxPopupContentRect(popup);

  const f32 itemsHeight =
      static_cast<f32>(itemCount) *
      ComboBoxItemHeight;

  f32 maxScroll =
      itemsHeight - content.h;

  if (maxScroll < 0.0f) {
    maxScroll = 0.0f;
  }

  return maxScroll;
}

} // namespace octogui::detail