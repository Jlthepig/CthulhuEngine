#include "Systems/PaintSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/PaintHelpers.h"
#include "Internal/TreeViewInternal.h"
#include "Internal/WidgetMetrics.h"

namespace octogui {
namespace {

Color treeViewAccentColor(const VisualStyle* style, const StateStyle& current) {
  Color color = current.indicator;

  if (color.isTransparent() && style && !style->accentEdge.color.isTransparent()) {
    color = style->accentEdge.color;
  }

  if (color.isTransparent() && style && !style->focusRing.color.isTransparent()) {
    color = style->focusRing.color;
  }

  if (color.isTransparent()) {
    color = current.text;
  }

  if (color.isTransparent()) {
    color = Color::white();
  }

  return color;
}

} // namespace

void PaintSystem::paintTreeView(
    Tree& tree, DrawList& drawList, Font* font, NodeHandle node, Rect rect) {
  TreeViewState* state = tree.treeViewState(node);

  if (!state) {
    return;
  }

  if (state->visibleItemsDirty) {
    detail::rebuildTreeViewVisibleItems(*state);
  }

  const Rect content = detail::contentRect(tree, node, rect);

  if (content.isEmpty()) {
    return;
  }

  detail::ensureTreeViewSelectionVisible(
      *state,
      content);

  detail::clampTreeViewScrollY(
      *state,
      content);

  const VisualStyle* visualStyle = tree.visualStyle(node);

  StateStyle currentState;

  if (visualStyle) {
    currentState = stateStyle(
        *visualStyle,
        tree.isEnabled(node) ? StyleState::Normal : StyleState::Disabled);
  } else {
    currentState.text = Color::white();
    currentState.indicator = Color::white();
  }

  Color textColor = currentState.text;

  if (textColor.isTransparent()) {
    textColor = Color::white();
  }

  const Color accentColor = treeViewAccentColor(visualStyle, currentState);
  const Color selectedBackground = accentColor.withAlpha(0.16f);
  const Color hoveredBackground = accentColor.withAlpha(0.08f);

  const f32 scrollY =
      state->scrollY;

  const usize visibleCount = state->visibleItems.size();

  if (visibleCount == 0) {
    return;
  }

  const f32 rowHeight = detail::TreeViewRowHeight;

  usize firstRow = static_cast<usize>(scrollY / rowHeight);

  if (firstRow >= visibleCount) {
    firstRow = visibleCount - 1;
  }

  usize lastRow = static_cast<usize>((scrollY + content.h) / rowHeight) + 1;

  if (lastRow > visibleCount) {
    lastRow = visibleCount;
  }

  const bool canPaintText =
      font && font->isLoaded();

  const f32 textHeight =
      canPaintText
          ? detail::fontTextHeight(
                font->metrics())
          : 0.0f;

  drawList.pushClip(content);

  for (usize rowIndex = firstRow; rowIndex < lastRow; ++rowIndex) {
    const TreeViewVisibleItem& visible = state->visibleItems[rowIndex];

    const TreeViewItemSlot* item = detail::treeViewItem(*state, visible.item);

    if (!item) {
      continue;
    }

    const Rect row{
        content.x,
        content.y + static_cast<f32>(rowIndex) * rowHeight - scrollY,
        content.w,
        rowHeight};

    const bool selected = state->selected == visible.item;
    const bool hovered = state->hovered == visible.item;

    if (selected) {
      drawList.addRoundedRectFilled(
          row, selectedBackground, detail::TreeViewRowCornerRadius);

      drawList.addRoundedRectFilled(
          Rect{
              row.x,
              row.y + 2.0f,
              detail::TreeViewSelectionAccentWidth,
              row.h - 4.0f},
          accentColor,
          detail::TreeViewSelectionAccentWidth * 0.5f);
    } else if (hovered) {
      drawList.addRoundedRectFilled(
          row, hoveredBackground, detail::TreeViewRowCornerRadius);
    }

    const f32 indentX =
        row.x + static_cast<f32>(visible.depth) * detail::TreeViewIndentWidth;

    const f32 chevronCenterX = indentX + detail::TreeViewChevronAreaWidth * 0.5f;
    const f32 chevronCenterY = row.y + row.h * 0.5f;

    const TreeViewItemSlot* firstChild =
        detail::treeViewItem(*state, item->firstChild);

    if (firstChild) {
      const f32 half = detail::TreeViewChevronSize * 0.5f;

      if (item->expanded) {
        const Vec2 left{chevronCenterX - half, chevronCenterY - half * 0.35f};
        const Vec2 middle{chevronCenterX, chevronCenterY + half * 0.35f};
        const Vec2 right{chevronCenterX + half, chevronCenterY - half * 0.35f};

        drawList.addLine(
            left, middle, textColor, detail::TreeViewChevronThickness);

        drawList.addLine(
            middle, right, textColor, detail::TreeViewChevronThickness);
      } else {
        const Vec2 top{chevronCenterX - half * 0.35f, chevronCenterY - half};
        const Vec2 middle{chevronCenterX + half * 0.35f, chevronCenterY};
        const Vec2 bottom{chevronCenterX - half * 0.35f, chevronCenterY + half};

        drawList.addLine(
            top, middle, textColor, detail::TreeViewChevronThickness);

        drawList.addLine(
            middle, bottom, textColor, detail::TreeViewChevronThickness);
      }
    }

    if (!canPaintText || item->label.empty()) {
      continue;
    }

    const f32 textX =
        indentX + detail::TreeViewChevronAreaWidth + detail::TreeViewTextGap;

    const f32 textWidth = row.maxX() - textX - 4.0f;

    if (textWidth <= 0.0f) {
      continue;
    }

    const f32 textY =
        detail::centeredTextTop(
            row,
            textHeight);

    const Rect textClip{textX, row.y, textWidth, row.h};

    font->drawTextClipped(
        drawList, item->label, Vec2{textX, textY}, textColor, textClip);
  }

  drawList.popClip();

  const auto scrollbar =
      detail::treeViewScrollbarGeometry(
          *state,
          tree,
          node);

  if (!scrollbar.visible) {
    return;
  }

  Color base =
      textColor;

  if (base.isTransparent()) {
    base =
        Color::white();
  }

  const Color trackColor =
      base.withAlpha(0.18f);

  f32 thumbAlpha =
      0.38f;

  if (state->scrollbarHovered) {
    thumbAlpha =
        0.56f;
  }

  if (state->scrollbarDragging) {
    thumbAlpha =
        0.76f;
  }

  const Color thumbColor =
      base.withAlpha(
          thumbAlpha);

  drawList.addRoundedRectFilled(
      scrollbar.track,
      trackColor,
      scrollbar.track.w *
          0.5f);

  drawList.addRoundedRectFilled(
      scrollbar.thumb,
      thumbColor,
      scrollbar.thumb.w *
          0.5f);
}

} // namespace octogui