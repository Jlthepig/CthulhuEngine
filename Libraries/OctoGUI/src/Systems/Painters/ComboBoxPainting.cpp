#include "Systems/PaintSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/PaintHelpers.h"
#include "Internal/WidgetMetrics.h"
#include "Systems/InteractionSystem.h"

namespace octogui {

void PaintSystem::paintComboBoxClosed(
    const Tree& tree,
    DrawList& drawList,
    Font& font,
    NodeHandle node,
    Rect rect,
    Color textColor) {
  const ComboBoxState* state =
      tree.comboBoxState(node);

  if (!state) {
    return;
  }

  const Rect content =
      detail::contentRect(tree, node, rect);

  if (content.isEmpty()) {
    return;
  }

  f32 chevronSize =
      detail::ComboBoxChevronSize;

  if (chevronSize > content.h) {
    chevronSize = content.h;
  }

  const f32 chevronCenterX =
      content.maxX() - chevronSize * 0.5f;

  const f32 chevronCenterY =
      content.y + content.h * 0.5f;

  const f32 chevronHalf =
      chevronSize * 0.5f;

  const f32 textRight =
      chevronCenterX -
      chevronHalf -
      detail::ComboBoxChevronGap;

  const f32 textWidth =
      textRight - content.x;

  if (textWidth > 0.0f &&
      state->selectedIndex >= 0 &&
      state->selectedIndex <
          static_cast<i32>(state->items.size()) &&
      !textColor.isTransparent()) {
    const std::string_view text =
        state->items[
            static_cast<usize>(
                state->selectedIndex)];

    if (!text.empty()) {
        const f32 textHeight =
          detail::fontTextHeight(
            font.metrics());

        const f32 textTop =
          detail::centeredTextTop(
            content,
            textHeight);

      const Rect textRect{
          content.x,
          content.y,
          textWidth,
          content.h};

      drawList.pushClip(textRect);

      font.drawText(
          drawList,
          text,
          Vec2{content.x, textTop},
          textColor);

      drawList.popClip();
    }
  }

  if (textColor.isTransparent()) {
    return;
  }

  const f32 direction =
    state->open ? -1.0f : 1.0f;

  const Vec2 left{
      chevronCenterX - chevronHalf,
      chevronCenterY -
          direction * chevronHalf * 0.35f};

  const Vec2 middle{
      chevronCenterX,
      chevronCenterY +
          direction * chevronHalf * 0.35f};

  const Vec2 right{
      chevronCenterX + chevronHalf,
      chevronCenterY -
          direction * chevronHalf * 0.35f};

  drawList.addLine(
      left,
      middle,
      textColor,
      detail::ComboBoxChevronThickness);

  drawList.addLine(
      middle,
      right,
      textColor,
      detail::ComboBoxChevronThickness);
}

void PaintSystem::trackComboBoxOverlay(
    NodeHandle handle) {
  for (const ComboBoxOverlay& overlay :
       comboBoxOverlays) {
    if (overlay.handle == handle) {
      return;
    }
  }

  comboBoxOverlays.push_back(
      ComboBoxOverlay{handle});
}

void PaintSystem::pruneComboBoxOverlays(
    const Tree& tree) {
  usize write = 0;

  for (usize read = 0;
       read < comboBoxOverlays.size();
       ++read) {
    const ComboBoxOverlay& overlay =
        comboBoxOverlays[read];

    if (!tree.isValid(overlay.handle)) {
      continue;
    }

    if (tree.type(overlay.handle) !=
        NodeType::ComboBox) {
      continue;
    }

    const ComboBoxState* state =
        tree.comboBoxState(overlay.handle);

    if (!state || !state->open) {
      continue;
    }

    comboBoxOverlays[write++] =
        overlay;
  }

  comboBoxOverlays.resize(write);
}

void PaintSystem::paintComboBoxOverlays(
    const Tree& tree,
    NodeHandle root,
     [[maybe_unused]] const InteractionState& interaction,
    Font* font) {
  if (!tree.isValid(root)) {
    return;
  }

  const Rect viewport =
      tree.rect(root);

  if (viewport.isEmpty()) {
    return;
  }

  for (const ComboBoxOverlay& overlay :
       comboBoxOverlays) {
    if (!tree.isValid(overlay.handle)) {
      continue;
    }

    const ComboBoxState* state =
        tree.comboBoxState(overlay.handle);

    if (!state ||
        !state->open ||
        state->items.empty()) {
      continue;
    }

    const Rect anchorRect =
      detail::visualRect(
        tree,
        overlay.handle);

    paintComboBoxPopup(
        tree,
        drawListData,
        font,
        overlay.handle,
        anchorRect,
        viewport);
  }
}

void PaintSystem::paintComboBoxPopup(
    const Tree& tree,
    DrawList& drawList,
    Font* font,
    NodeHandle node,
    Rect anchorRect,
    Rect viewport) {
  const ComboBoxState* combo =
      tree.comboBoxState(node);

  if (!combo ||
      !combo->open ||
      combo->items.empty()) {
    return;
  }

  const Rect popup =
      detail::comboBoxPopupRect(
          anchorRect,
          viewport,
          combo->items.size());

    const f32 maxScroll =
      detail::comboBoxMaxScrollY(
        popup,
        combo->items.size());

    const f32 scrollY =
      combo->popupScrollY < maxScroll
        ? combo->popupScrollY
        : maxScroll;
  
  const Rect content =
    detail::comboBoxPopupContentRect(
        popup);

  if (content.isEmpty()) {
    return;
  }

  if (popup.isEmpty()) {
    return;
  }

  const VisualStyle* style =
      tree.visualStyle(node);

  StateStyle normal;

  if (style) {
    normal =
        stateStyle(
            *style,
            StyleState::Normal);
  } else {
    normal.background =
        Color::black();

    normal.border =
        Color::white();

    normal.text =
        Color::white();
  }

  Color background =
      normal.background;

  if (background.isTransparent()) {
    background =
        Color::black().withAlpha(0.95f);
  }

  Color border =
      normal.border;

  if (border.isTransparent()) {
    border =
        Color::white().withAlpha(0.15f);
  }

  Color textColor =
      normal.text;

  if (textColor.isTransparent()) {
    textColor =
        Color::white();
  }

  const f32 radius =
      style
          ? style->cornerRadius
          : detail::ComboBoxPopupCornerRadius;

  const f32 borderWidth =
      style
          ? style->borderWidth
          : 1.0f;

  drawList.addBoxShadow(
      popup,
      Color::black().withAlpha(0.35f),
      radius,
      Vec2{0.0f, 4.0f},
      12.0f,
      0.0f);

  drawList.addRoundedRectFilled(
      popup,
      background,
      radius);

  if (borderWidth > 0.0f &&
      !border.isTransparent()) {
    drawList.addRoundedRectOutline(
        popup,
        border,
        radius,
        borderWidth);
  }

  if (!font || !font->isLoaded()) {
    return;
  }

    const f32 textHeight =
      detail::fontTextHeight(
        font->metrics());

  drawList.pushClip(content);

  for (usize i = 0;
       i < combo->items.size();
       ++i) {
    const Rect itemRect =
        detail::comboBoxItemRect(
            popup,
            i,
          scrollY);

    if (itemRect.maxY() <= content.y ||
        itemRect.y >= content.maxY()) {
      continue;
    }

    const bool selected =
      static_cast<i32>(i) ==
      combo->selectedIndex;

    const bool hovered =
      static_cast<i32>(i) ==
      combo->hoveredIndex;

    if (selected || hovered) {
      Color itemColor =
        normal.indicator;

      if (itemColor.isTransparent()) {
      itemColor =
        textColor.withAlpha(
          hovered ? 0.12f : 0.08f);
      } else {
      itemColor =
        itemColor.withAlpha(
          hovered ? 0.22f : 0.14f);
      }

      drawList.addRoundedRectFilled(
        itemRect,
        itemColor,
        radius * 0.6f);
    }

    const std::string& item =
        combo->items[i];

    if (item.empty()) {
      continue;
    }

    const f32 textTop =
      detail::centeredTextTop(
        itemRect,
        textHeight);

    const Rect textRect{
        itemRect.x + 8.0f,
        itemRect.y,
        itemRect.w - 16.0f,
        itemRect.h};

    if (textRect.w <= 0.0f) {
      continue;
    }

    drawList.pushClip(textRect);

    font->drawText(
        drawList,
        item,
        Vec2{
            textRect.x,
            textTop},
        textColor);

    drawList.popClip();
  }

  drawList.popClip();

    if (maxScroll > 0.0f) {
    const f32 totalHeight =
      static_cast<f32>(
        combo->items.size()) *
      detail::ComboBoxItemHeight;

    f32 thumbHeight =
      content.h *
      (content.h / totalHeight);

    if (thumbHeight <
      detail::ComboBoxScrollbarMinThumbHeight) {
      thumbHeight =
        detail::ComboBoxScrollbarMinThumbHeight;
    }

    if (thumbHeight > content.h) {
      thumbHeight = content.h;
    }

    const f32 travel =
      content.h - thumbHeight;

    const f32 t =
      maxScroll > 0.0f
        ? scrollY /
            maxScroll
        : 0.0f;

    const Rect thumb{
      popup.maxX() -
        detail::ComboBoxScrollbarInset -
        detail::ComboBoxScrollbarWidth,
      content.y + travel * t,
      detail::ComboBoxScrollbarWidth,
      thumbHeight};

    Color thumbColor =
      normal.border;

    if (thumbColor.isTransparent()) {
      thumbColor =
        textColor.withAlpha(0.35f);
    }

    drawList.addRoundedRectFilled(
      thumb,
      thumbColor,
      detail::ComboBoxScrollbarWidth * 0.5f);
    }
}

} // namespace octogui
