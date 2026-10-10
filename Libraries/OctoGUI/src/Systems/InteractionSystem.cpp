#include "Systems/InteractionSystem.h"
#include "Systems/EventSystem.h"
#include "Systems/WidgetSystem.h"

namespace octogui {

const InteractionState &InteractionSystem::state() const noexcept {
  return interaction;
}

void InteractionSystem::resetTransientState() noexcept {
  interaction.pressed = NodeHandle{};
  interaction.released = NodeHandle{};
  interaction.clicked = NodeHandle{};
}

void InteractionSystem::validateTrackedNodes(Tree& tree, WidgetSystem& widgets, EventSystem& events) {
  if (!tree.isValid(interaction.active) ||
      !tree.isVisible(interaction.active) ||
      !isEnabledBranch(tree, interaction.active) ||
      !hasFlag(tree.flags(interaction.active), NodeFlags::Interactive)) {
    interaction.active = NodeHandle{};
  }

  if (!tree.isValid(interaction.focused) ||
      !tree.isVisible(interaction.focused) ||
      !isEnabledBranch(tree, interaction.focused)) {
    clearFocus(tree, widgets, events);
  }

  if (!tree.isValid(interaction.splitterActive) ||
      tree.type(interaction.splitterActive) != NodeType::SplitContainer) {
    interaction.splitterActive = NodeHandle{};
    splitterGrabOffset = 0.0f;
  }

  if (!tree.isValid(interaction.scrollbarActive)) {
    interaction.scrollbarActive = NodeHandle{};
  } else {
    const LayoutStyle* style =
        tree.layoutStyle(interaction.scrollbarActive);

    if (!style || !style->scrollY ||
        tree.maxScrollOffsetY(interaction.scrollbarActive) <= 0.0f) {
      interaction.scrollbarActive = NodeHandle{};
    }
  }
}

void InteractionSystem::markInteractionChanges(Tree& tree, const InteractionState& previous) {
  markStateChangeDirty(
      tree,
      previous.hovered,
      interaction.hovered);

  markStateChangeDirty(
      tree,
      previous.active,
      interaction.active);

  markStateChangeDirty(
      tree,
      previous.scrollbarHovered,
      interaction.scrollbarHovered);

  markStateChangeDirty(
      tree,
      previous.scrollbarActive,
      interaction.scrollbarActive);

  markStateChangeDirty(
      tree,
      previous.splitterHovered,
      interaction.splitterHovered);

  markStateChangeDirty(
      tree,
      previous.splitterActive,
      interaction.splitterActive);
}

void InteractionSystem::update(Tree& tree, NodeHandle root, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font) {
  const InteractionState previous = interaction;

  resetTransientState();

  if (!tree.isValid(root)) {
    interaction.hovered = NodeHandle{};
    interaction.active = NodeHandle{};
    interaction.scrollbarHovered = NodeHandle{};
    interaction.scrollbarActive = NodeHandle{};
    interaction.splitterHovered = NodeHandle{};
    interaction.splitterActive = NodeHandle{};

    clearFocus(tree, widgets, events);
    markInteractionChanges(tree, previous);

    return;
  }

  validateTrackedNodes(tree, widgets, events);

  updatePointerTargets(tree, root, input, widgets, events);

  handleLeftPress(tree, input, widgets, events, font);
  updateActivePointerDrag(tree, input, widgets, events, font);
  handleLeftRelease(tree, input, widgets, events, font);

  if (tree.isValid(interaction.active) &&
      !input.mouse.isDown(MouseButton::Left)) {
    interaction.active = NodeHandle{};
  }

  updateFocusedEditing(tree, input, widgets, events, font);

  markInteractionChanges(tree, previous);
}

void InteractionSystem::onNodeDestroyed(Tree &tree, EventSystem &events,
                                        NodeHandle handle) noexcept {
  if (!tree.isValid(handle)) {
    return;
  }

  if (interaction.hovered == handle) {
    interaction.hovered = NodeHandle{};
  }
  if (interaction.active == handle) {
    interaction.active = NodeHandle{};
  }

  if (interaction.focused == handle) {
    const NodeHandle previous =
        interaction.focused;

    interaction.focused =
        NodeHandle{};

    markStateChangeDirty(tree,previous,interaction.focused);
    
    events.push(UIEventType::FocusLost,previous);
  }

  if (interaction.pressed == handle) {
    interaction.pressed = NodeHandle{};
  }
  if (interaction.released == handle) {
    interaction.released = NodeHandle{};
  }
  if (interaction.clicked == handle) {
    interaction.clicked = NodeHandle{};
  }

  if (interaction.scrollbarHovered == handle) {
    interaction.scrollbarHovered = NodeHandle{};
  }

  if (interaction.scrollbarActive == handle) {
    interaction.scrollbarActive = NodeHandle{};
    scrollbarGrabOffsetY = 0.0f;
  }

  if (interaction.splitterHovered == handle) {
    interaction.splitterHovered = NodeHandle{};
  }

  if (interaction.splitterActive == handle) {
    interaction.splitterActive = NodeHandle{};
    splitterGrabOffset = 0.0f;
  }
}

} // namespace octogui
