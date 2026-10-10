#include "Systems/InteractionSystem.h"
#include "Systems/EventSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/SplitLayoutInternal.h"
#include "Systems/Interaction/InteractionInternal.h"
#include "Internal/WidgetMetrics.h"

namespace octogui {

void InteractionSystem::updatePointerTargets(Tree& tree, NodeHandle root, const InputState& input, WidgetSystem& widgets, EventSystem& events) {
  const NodeHandle previousHovered = interaction.hovered;

  zOrderScratch.clear();

  NodeHandle scrollbarHit =
      hitTestScrollbar(tree, root, input.mouse.position, Vec2::zero());

  if (tree.isValid(interaction.scrollbarActive)) {
    scrollbarHit = interaction.scrollbarActive;
  }

  interaction.scrollbarHovered = scrollbarHit;

  NodeHandle splitterHit{};

  if (!tree.isValid(scrollbarHit) &&
      !tree.isValid(interaction.scrollbarActive)) {
    zOrderScratch.clear();
    splitterHit = hitTestSplitter(tree, root, input.mouse.position,
                                   Vec2::zero());
  }

  if (tree.isValid(interaction.splitterActive)) {
    splitterHit = interaction.splitterActive;
  }

  interaction.splitterHovered = splitterHit;

  zOrderScratch.clear();

  const bool overComboBoxPopup =
    updateComboBoxPopupHover(
        tree,
        root,
        widgets,
        input.mouse.position);

    if (overComboBoxPopup &&
      input.mouse.wheel.y != 0.0f &&
      tree.isValid(interaction.focused) &&
      tree.type(interaction.focused) ==
        NodeType::ComboBox) {
    ComboBoxState* state =
      tree.comboBoxState(
        interaction.focused);

    if (state &&
      state->open &&
      !state->items.empty()) {
      const Rect viewport =
        tree.rect(root);

      const Rect anchor =
        detail::visualRect(
          tree,
          interaction.focused);

      const Rect popup =
        detail::comboBoxPopupRect(
          anchor,
          viewport,
          state->items.size());

      const f32 maxScroll =
        detail::comboBoxMaxScrollY(
          popup,
          state->items.size());

      const f32 delta =
        -input.mouse.wheel.y *
        detail::ComboBoxItemHeight *
        detail::ComboBoxWheelItems;

      widgets.setComboBoxScrollY(
        tree,
        interaction.focused,
        state->popupScrollY + delta,
        maxScroll);

      (void)updateComboBoxPopupHover(
        tree,
        root,
        widgets,
        input.mouse.position);
    }
    }

  NodeHandle hit{};

  if (!tree.isValid(scrollbarHit) && !tree.isValid(splitterHit)) {
    if (overComboBoxPopup &&
        tree.isValid(interaction.focused)) {
      hit = interaction.focused;
    } else {
      hit = hitTest(
          tree,
          root,
          input.mouse.position,
          Vec2::zero());
    }
  }

    if (!overComboBoxPopup &&
      !tree.isValid(interaction.scrollbarActive) &&
      input.mouse.wheel.y != 0.0f) {
    const f32 wheelY =
        input.mouse.wheel.y;

    bool wheelConsumed = false;

    if (tree.isValid(
            interaction.hovered) &&
        tree.type(
            interaction.hovered) ==
            NodeType::TreeView) {
      wheelConsumed =
          widgets.scrollTreeView(
              tree,
              interaction.hovered,
              wheelY);
    }

    if (!wheelConsumed) {
      const NodeHandle scrollStart =
          tree.isValid(scrollbarHit) ? scrollbarHit : hit;

      if (tree.isValid(scrollStart) &&
          applyWheelScroll(tree, scrollStart, wheelY)) {
        zOrderScratch.clear();

        scrollbarHit =
            hitTestScrollbar(tree, root, input.mouse.position, Vec2::zero());

        interaction.scrollbarHovered = scrollbarHit;

        zOrderScratch.clear();

        if (tree.isValid(scrollbarHit)) {
          hit = NodeHandle{};
        } else {
          hit = hitTest(tree, root, input.mouse.position, Vec2::zero());
        }
      }
    }
  }

  if (tree.isValid(hit) && isEnabledBranch(tree, hit)) {
    interaction.hovered = findInteractive(tree, hit);
  } else {
    interaction.hovered = NodeHandle{};
  }

  if (tree.isValid(interaction.hovered) &&
      tree.type(interaction.hovered) ==
          NodeType::TreeView) {
    widgets.updateTreeViewHover(
        tree,
        interaction.hovered,
        input.mouse.position);
  }

  if (tree.isValid(previousHovered) &&
      tree.type(previousHovered) ==
          NodeType::TreeView &&
      previousHovered !=
          interaction.hovered) {
    widgets.clearTreeViewHover(
        tree,
        previousHovered);
  }

  if (previousHovered == interaction.hovered) {
    return;
  }

  if (tree.isValid(previousHovered)) {
    events.pushPointer(
        UIEventType::PointerLeave,
        previousHovered,
        input.mouse.position);
  }

  if (tree.isValid(interaction.hovered)) {
    events.pushPointer(
        UIEventType::PointerEnter,
        interaction.hovered,
        input.mouse.position);
  }
}

void InteractionSystem::handleLeftPress(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font) {
  if (!input.mouse.pressed(MouseButton::Left)) {
    return;
  }

  if (tree.isValid(interaction.scrollbarHovered)) {
    interaction.active = NodeHandle{};

    beginScrollbarInteraction(
        tree,
        interaction.scrollbarHovered,
        input.mouse.position);

    return;
  }

  if (tree.isValid(interaction.splitterHovered)) {
    interaction.active = {};
    beginSplitterInteraction(tree, interaction.splitterHovered,
                             input.mouse.position);
    return;
  }

  if (!tree.isValid(interaction.hovered)) {
    interaction.active = NodeHandle{};
    clearFocus(tree, widgets, events);
    return;
  }

  interaction.pressed = interaction.hovered;
  interaction.active = interaction.hovered;

  events.pushPointer(
      UIEventType::PointerPressed,
      interaction.pressed,
      input.mouse.position);

  const bool wasFocused =
      interaction.focused == interaction.hovered;

  if (hasFlag(tree.flags(interaction.hovered), NodeFlags::Focusable)) {
    changeFocus(tree, widgets, events, interaction.hovered);
  }

  const NodeType activeType = tree.type(interaction.active);

  if (activeType == NodeType::TreeView) {
    widgets.beginTreeViewPress(
        tree,
        interaction.active,
        input.mouse.position);
  }

  if (interaction_detail::isTextEditingType(activeType) && font && font->isLoaded()) {
    widgets.beginTextInputSelection(
        tree,
        *font,
        interaction.active,
        input.mouse.position,
        wasFocused && input.modifiers.shift);
  }

  if (activeType == NodeType::NumericInput) {
    widgets.beginNumericInputDrag(
        tree,
        interaction.active,
        input.mouse.position);
  }
}

void InteractionSystem::updateActivePointerDrag(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font) {
  if (!input.mouse.isDown(MouseButton::Left)) {
    return;
  }

  if (tree.isValid(interaction.scrollbarActive)) {
    updateScrollbarDrag(
        tree,
        interaction.scrollbarActive,
        input.mouse.position);

    return;
  }

  if (tree.isValid(interaction.splitterActive)) {
    updateSplitterDrag(tree, widgets, interaction.splitterActive,
                       input.mouse.position);
    return;
  }

  if (!tree.isValid(interaction.active)) {
    return;
  }

  const NodeType activeType =
      tree.type(interaction.active);

    if (tree.isValid(
        interaction.active) &&
      activeType == NodeType::TreeView &&
      input.mouse.isDown(
        MouseButton::Left)) {
    widgets.updateTreeViewPress(
      tree,
      interaction.active,
      input.mouse.position);
    }

  if (activeType == NodeType::Slider) {
    widgets.updateSliderFromMouse(
        tree,
        events,
        interaction.active,
        input.mouse.position);

    return;
  }

  if (activeType == NodeType::NumericInput) {
    const bool consumed =
        widgets.updateNumericInputDrag(
            tree,
            events,
            interaction.active,
            input.mouse.position);

    if (!consumed && font && font->isLoaded()) {
      widgets.updateTextInputSelection(
          tree,
          *font,
          interaction.active,
          input.mouse.position,
          input.deltaTime);
    }

    return;
  }

  if (activeType == NodeType::TextInput &&
      font && font->isLoaded()) {
    widgets.updateTextInputSelection(
        tree,
        *font,
        interaction.active,
        input.mouse.position,
        input.deltaTime);
  }
}

void InteractionSystem::handleLeftRelease(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font) {
  if (!input.mouse.released(MouseButton::Left)) {
    return;
  }

  if (tree.isValid(interaction.scrollbarActive)) {
    updateScrollbarDrag(
        tree,
        interaction.scrollbarActive,
        input.mouse.position);

    interaction.scrollbarActive = NodeHandle{};
    return;
  }

  if (tree.isValid(interaction.splitterActive)) {
    const NodeHandle split = interaction.splitterActive;

    updateSplitterDrag(tree, widgets, split, input.mouse.position);
    interaction.splitterActive = {};
    tree.markPaintDirty(split);
    return;
  }

  if (tree.isValid(interaction.hovered)) {
    interaction.released = interaction.hovered;

    events.pushPointer(
        UIEventType::PointerReleased,
        interaction.released,
        input.mouse.position);
  }

  bool suppressClick = false;

  if (tree.isValid(interaction.active)) {
    const NodeType activeType =
        tree.type(interaction.active);

    if (activeType == NodeType::Slider) {
      widgets.updateSliderFromMouse(
          tree,
          events,
          interaction.active,
          input.mouse.position);
    }

    if (activeType == NodeType::NumericInput) {
      widgets.updateNumericInputDrag(
          tree,
          events,
          interaction.active,
          input.mouse.position);

      const NumericInputState* numericState =
          tree.numericInputState(
              interaction.active);

      suppressClick =
          numericState &&
          numericState->dragging;

      widgets.endTextInputSelection(
          tree,
          interaction.active);

      widgets.endNumericInputDrag(
          tree,
          interaction.active);
    } else if (activeType == NodeType::TextInput) {
      if (font && font->isLoaded()) {
        widgets.updateTextInputSelection(
            tree,
            *font,
            interaction.active,
            input.mouse.position,
            input.deltaTime);
      }

      widgets.endTextInputSelection(
          tree,
          interaction.active);
    }
  }

  const NodeHandle releasedActive =
      interaction.active;

  if (tree.isValid(releasedActive) &&
      tree.type(releasedActive) ==
          NodeType::TreeView) {
    widgets.endTreeViewPress(
        tree,
        events,
        releasedActive,
        input.mouse.position);

    interaction.active = NodeHandle{};
    return;
  }

    if (tree.isValid(interaction.active) &&
      tree.type(interaction.active) ==
        NodeType::ComboBox) {
    ComboBoxState* state =
      tree.comboBoxState(
        interaction.active);

    if (state &&
      state->open &&
      state->hoveredIndex >= 0) {
      const NodeHandle combo =
        interaction.active;

      interaction.released =
        combo;

      events.pushPointer(
        UIEventType::PointerReleased,
        combo,
        input.mouse.position);

      if (widgets.selectComboBoxHovered(
          tree,
          events,
          combo)) {
      interaction.clicked =
        combo;

      events.pushPointer(
        UIEventType::Clicked,
        combo,
        input.mouse.position);

      interaction.active =
        NodeHandle{};

      return;
      }
    }
    }

  if (!suppressClick &&
      tree.isValid(interaction.active) &&
      interaction.hovered == interaction.active) {
    interaction.clicked =
        interaction.active;
  }

  if (tree.isValid(interaction.clicked)) {
    events.pushPointer(
        UIEventType::Clicked,
        interaction.clicked,
        input.mouse.position);

    widgets.handleClick(
        tree,
        events,
        interaction.clicked);
  }

  interaction.active = NodeHandle{};
}

void InteractionSystem::beginSplitterInteraction(Tree &tree, NodeHandle node,
                                                 Vec2 mousePosition) {
  if (!tree.isValid(node) || tree.type(node) != NodeType::SplitContainer) {
    return;
  }

  const SplitContainerState *state = tree.splitContainerState(node);

  if (!state) {
    return;
  }

  const Rect rect = detail::visualRect(tree, node);
  const detail::SplitGeometry geometry =
      detail::splitGeometry(tree, node, rect);

  if (geometry.divider.isEmpty()) {
    return;
  }

  interaction.splitterActive = node;

  if (state->orientation == SplitOrientation::Horizontal) {
    splitterGrabOffset =
        mousePosition.x - (geometry.divider.x + geometry.divider.w * 0.5f);
  } else {
    splitterGrabOffset =
        mousePosition.y - (geometry.divider.y + geometry.divider.h * 0.5f);
  }

  tree.markPaintDirty(node);
}

void InteractionSystem::updateSplitterDrag(Tree &tree, WidgetSystem &widgets,
                                           NodeHandle node,
                                           Vec2 mousePosition) {
  if (!tree.isValid(node) || tree.type(node) != NodeType::SplitContainer) {
    return;
  }

  const SplitContainerState *state = tree.splitContainerState(node);

  if (!state) {
    return;
  }

  const Rect visualRect = detail::visualRect(tree, node);
  const Rect content = detail::splitContentRect(tree, node, visualRect);
  const f32 mainSize = state->orientation == SplitOrientation::Horizontal
                           ? content.w
                           : content.h;
  const f32 available = mainSize - detail::SplitDividerWidth;

  if (available <= 0.0f) {
    return;
  }

  f32 firstSize;

  if (state->orientation == SplitOrientation::Horizontal) {
    const f32 dividerCenter = mousePosition.x - splitterGrabOffset;
    firstSize = dividerCenter - content.x - detail::SplitDividerWidth * 0.5f;
  } else {
    const f32 dividerCenter = mousePosition.y - splitterGrabOffset;
    firstSize = dividerCenter - content.y - detail::SplitDividerWidth * 0.5f;
  }

  const f32 ratio = detail::constrainedSplitRatio(
      tree, node, content, firstSize / available);

  widgets.setSplitRatio(tree, node, ratio);
}

} // namespace octogui
