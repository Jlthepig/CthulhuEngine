#include "Systems/PaintSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/PaintHelpers.h"
#include "Internal/WidgetMetrics.h"
#include "Internal/ZOrder.h"
#include "Systems/InteractionSystem.h"
namespace octogui {
namespace {

void addAccentEdge(DrawList &list, Rect rect, const AccentEdgeStyle &edge) {
  if (edge.width <= 0.0f || edge.color.isTransparent()) {
    return;
  }

  const f32 inset = edge.inset > 0.0f ? edge.inset : 0.0f;
  Rect edgeRect{};

  switch (edge.side) {
  case AccentEdgeSide::Left:
    edgeRect = Rect{rect.x, rect.y + inset, edge.width, rect.h - inset * 2.0f};
    break;

  case AccentEdgeSide::Right:
    edgeRect = Rect{rect.x + rect.w - edge.width, rect.y + inset, edge.width,
                    rect.h - inset * 2.0f};
    break;

  case AccentEdgeSide::Top:
    edgeRect = Rect{rect.x + inset, rect.y, rect.w - inset * 2.0f, edge.width};
    break;

  case AccentEdgeSide::Bottom:
    edgeRect = Rect{rect.x + inset, rect.y + rect.h - edge.width,
                    rect.w - inset * 2.0f, edge.width};
    break;
  }

  if (edgeRect.isEmpty()) {
    return;
  }

  list.addRoundedRectFilled(edgeRect, edge.color, edge.width * 0.5f);
}

void paintVerticalScrollbar(const Tree &tree, DrawList &drawList,
                            NodeHandle node,
                            const InteractionState &interaction, Rect rect) {
  const LayoutStyle *layoutStyle = tree.layoutStyle(node);
  const LayoutResult *layoutResult = tree.layoutResult(node);

  if (!layoutStyle || !layoutStyle->scrollY || !layoutResult) {
    return;
  }

  const Rect viewport = detail::verticalScrollbarViewport(rect, layoutStyle->padding);

  const detail::VerticalScrollbarGeometry geometry =
      detail::verticalScrollbarGeometry(viewport, layoutResult->contentSize.y,
                                        tree.scrollOffsetY(node),
                                        tree.maxScrollOffsetY(node));

  if (!geometry.visible) {
    return;
  }

  Color base = Color::white();
  Color trackColor = Color::white().withAlpha(0.12f);

  if (const VisualStyle *visualStyle = tree.visualStyle(node)) {
    const StateStyle &normal = stateStyle(*visualStyle, StyleState::Normal);

    base = normal.text;

    if (base.isTransparent()) {
      base = normal.border;
    }

    if (base.isTransparent()) {
      base = Color::white();
    }

    Color trackBase = normal.border;

    if (trackBase.isTransparent()) {
      trackBase = base;
    }

    trackColor = trackBase.withAlpha(0.18f);
  }

  f32 thumbAlpha = 0.38f;

  if (interaction.scrollbarHovered == node) {
    thumbAlpha = 0.56f;
  }

  if (interaction.scrollbarActive == node) {
    thumbAlpha = 0.76f;
  }

  const Color thumbColor = base.withAlpha(thumbAlpha);

  const f32 trackRadius = geometry.track.w * 0.5f;
  const f32 thumbRadius = geometry.thumb.w * 0.5f;

  drawList.addRoundedRectFilled(geometry.track, trackColor, trackRadius);

  drawList.addRoundedRectFilled(geometry.thumb, thumbColor, thumbRadius);
}

} // namespace

DrawList &PaintSystem::drawList() noexcept { return drawListData; }

const DrawList &PaintSystem::drawList() const noexcept { return drawListData; }

void PaintSystem::beginFrame() noexcept { drawListData.clear(); }

void PaintSystem::paint(Tree &tree, NodeHandle root,
                        const InteractionState &interaction, Font *font, const IconAtlas& icons) {
  if (!tree.isValid(root)) {
    return;
  }

  zOrderScratch.clear();

  if (paintCache.size() < tree.capacity()) {
    paintCache.resize(tree.capacity());
  }

  pruneComboBoxOverlays(tree);

  paintNode(tree, drawListData, root, interaction, font, icons, Vec2::zero());

  paintComboBoxOverlays(tree, root, interaction, font);
}

DrawList &PaintSystem::paintCacheFor(NodeHandle handle) {
  PaintCacheEntry &entry = paintCache[handle.index];

  if (entry.generation != handle.generation) {
    entry.generation = handle.generation;
    entry.commands.clear();
  }

  return entry.commands;
}

void PaintSystem::paintNode(Tree &tree, DrawList &drawList, NodeHandle node,
                            const InteractionState &interaction, Font *font,
                            const IconAtlas& icons, Vec2 appendOffset) {
  if (!tree.isValid(node) || !tree.isVisible(node)) {
    return;
  }

  if (tree.type(node) == NodeType::ComboBox) {
    const ComboBoxState* state =
        tree.comboBoxState(node);

    if (state && state->open) {
      trackComboBoxOverlay(node);
    }
  }

  DrawList &cache = paintCacheFor(node);

  const bool selfDirty = tree.isPaintDirty(node);
  const bool childrenDirty = tree.hasDirtyPaintChildren(node);

  if (!selfDirty && !childrenDirty && !cache.empty()) {
    drawList.appendTranslated(cache.span(), appendOffset);
    return;
  }

  cache.clear();

  const Rect rect = tree.rect(node);

  if (tree.type(node) == NodeType::Checkbox) {
    paintCheckbox(tree, cache, node, interaction, font, rect);
    tree.clearPaintDirty(node);
    tree.clearStyleDirty(node);
    drawList.appendTranslated(cache.span(), appendOffset);
    return;
  }

  if (tree.type(node) == NodeType::Slider) {
    paintSlider(tree, cache, node, interaction, rect);
    tree.clearPaintDirty(node);
    tree.clearStyleDirty(node);
    drawList.appendTranslated(cache.span(), appendOffset);
    return;
  }

  if (tree.type(node) == NodeType::ProgressBar) {
    paintProgressBar(tree, cache, node, font, rect);

    tree.clearPaintDirty(node);
    tree.clearStyleDirty(node);

    drawList.appendTranslated(cache.span(), appendOffset);

    return;
  }

  if (tree.type(node) == NodeType::RadioButton) {
    paintRadioButton(tree, cache, node, interaction, font, rect);
    tree.clearPaintDirty(node);
    tree.clearStyleDirty(node);
    drawList.appendTranslated(cache.span(), appendOffset);
    return;
  }

  if (tree.type(node) == NodeType::Separator) {
    paintSeparator(tree, cache, node, interaction, rect);
    tree.clearPaintDirty(node);
    tree.clearStyleDirty(node);
    drawList.appendTranslated(cache.span(), appendOffset);
    return;
  }

  if (tree.type(node) == NodeType::Icon) {
    paintIcon(tree, cache, node, icons, rect);
    tree.clearPaintDirty(node);
    tree.clearStyleDirty(node);
    drawList.appendTranslated(cache.span(),appendOffset);
    return;
  }

  const VisualStyle *visualStyle = tree.visualStyle(node);

  Color textColor = Color::white();

  Rect paintRect = rect;

  if (visualStyle) {
    StyleState styleState;

    if (tree.type(node) ==
        NodeType::TreeView) {
      styleState =
          tree.isEnabled(node)
              ? StyleState::Normal
              : StyleState::Disabled;
    } else {
      styleState =
          detail::resolveStyleState(
              tree,
              node,
              interaction);
    }

    const StateStyle &currentState = stateStyle(*visualStyle, styleState);

    if (styleState == StyleState::Active &&
        visualStyle->pressedOffsetY != 0.0f) {
      paintRect.y += visualStyle->pressedOffsetY;
    }

    textColor = currentState.text;

    const ShadowStyle &shadow = visualStyle->shadow;

    if (!shadow.color.isTransparent()) {
      cache.addBoxShadow(rect, shadow.color, visualStyle->cornerRadius,
                         shadow.offset, shadow.blur, shadow.spread);
    }

    if (visualStyle->cornerRadius > 0.0f) {
      if (currentState.gradient.enabled) {
        cache.addGradientRoundedRect(paintRect, currentState.gradient,
                                     visualStyle->cornerRadius);
      } else {
        cache.addRoundedRectFilled(paintRect, currentState.background,
                                   visualStyle->cornerRadius);
      }

      cache.addRoundedRectOutline(paintRect, currentState.border,
                                  visualStyle->cornerRadius,
                                  visualStyle->borderWidth);
    } else {
      if (currentState.gradient.enabled) {
        cache.addGradientRect(paintRect, currentState.gradient);
      } else {
        cache.addRectFilled(paintRect, currentState.background);
      }

      cache.addRectOutline(paintRect, currentState.border,
                           visualStyle->borderWidth);
    }

    addAccentEdge(cache, paintRect, visualStyle->accentEdge);
    detail::addFocusRing(
      cache,
      rect,
      *visualStyle,
      styleState);
  }

  const LayoutStyle *layoutStyle = tree.layoutStyle(node);

  const bool clipped =
      layoutStyle && (layoutStyle->clip || layoutStyle->scrollY);

  Rect clipRect = rect;

  if (layoutStyle && layoutStyle->scrollY) {
    clipRect = detail::contentRect(tree, node, rect);
  }

  if (clipped) {
    cache.pushClip(clipRect);
  }

  if (tree.type(node) == NodeType::TreeView) {
    paintTreeView(tree, cache, font, node, rect);
  } else if (font && font->isLoaded()) {
    if (tree.type(node) == NodeType::ComboBox) {
      paintComboBoxClosed(tree, cache, *font, node, paintRect, textColor);
    } else {
      paintNodeText(tree, cache, *font, node, paintRect, textColor, interaction);
    }
  }

  if (tree.type(node) ==
      NodeType::CollapsibleSection) {
    paintCollapsibleSectionIndicator(
        tree,
        cache,
        node,
        paintRect,
        textColor);
  }

  const detail::ZOrderRange range =
      detail::appendChildrenByZ(tree, node, zOrderScratch);

  Vec2 childOffset = Vec2::zero();

  if (layoutStyle && layoutStyle->scrollY) {
    childOffset.y = -tree.scrollOffsetY(node);
  }

  for (usize i = range.begin; i < range.end; ++i) {
    const NodeHandle child = zOrderScratch[i];
    paintNode(tree, cache, child, interaction, font, icons, childOffset);
  }

  if (tree.type(node) == NodeType::SplitContainer) {
    paintSplitContainerDivider(tree, cache, node, interaction, rect);
  }

  if (clipped) {
    cache.popClip();
  }

  if (layoutStyle && layoutStyle->scrollY) {
    paintVerticalScrollbar(tree, cache, node, interaction, rect);
  }

  tree.clearPaintDirty(node);
  tree.clearStyleDirty(node);

  drawList.appendTranslated(cache.span(), appendOffset);
}

} // namespace octogui
