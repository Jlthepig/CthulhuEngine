#include "Systems/InteractionSystem.h"
#include "Systems/EventSystem.h"
#include "Systems/WidgetSystem.h"

#include "Systems/Interaction/InteractionInternal.h"

namespace octogui {

void InteractionSystem::updateFocusedEditing(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font) {
  if (!tree.isValid(interaction.focused) ||
      !interaction_detail::isTextEditingType(tree.type(interaction.focused))) {
    return;
  }

  widgets.updateTextInputVisualState(
      tree,
      interaction.focused,
      input.deltaTime);

  if (!font || !font->isLoaded()) {
    return;
  }

  const NodeType focusedType =
      tree.type(interaction.focused);

  for (const InputEvent& event : input.events) {
    if (focusedType == NodeType::NumericInput) {
      widgets.handleNumericInputEvent(
          tree,
          *font,
          events,
          interaction.focused,
          event,
          input.clipboard);

      continue;
    }

    widgets.handleTextInputEvent(
        tree,
        *font,
        interaction.focused,
        event,
        input.clipboard);
  }
}

void InteractionSystem::setFocused(Tree& tree, WidgetSystem& widgets, EventSystem& events, NodeHandle handle) {
  if (!tree.isValid(handle)) {
    return;
  }

  if (!hasFlag(
          tree.flags(handle),
          NodeFlags::Focusable)) {
    return;
  }

  changeFocus(
      tree,
      widgets,
      events,
      handle);
}

void InteractionSystem::clearFocus(Tree& tree, WidgetSystem& widgets, EventSystem& events) {
  changeFocus(
      tree,
      widgets,
      events,
      NodeHandle{});
}

void InteractionSystem::changeFocus(Tree& tree, WidgetSystem& widgets, EventSystem& events, NodeHandle handle) {
  if (interaction.focused == handle) {
    return;
  }

  const NodeHandle previous =
      interaction.focused;

  if (tree.isValid(previous) &&
      tree.type(previous) == NodeType::NumericInput) {
    widgets.commitNumericInput(
        tree,
        events,
        previous);
  }

  if (tree.isValid(previous) &&
      tree.type(previous) ==
          NodeType::ComboBox) {
    widgets.setComboBoxOpen(
        tree,
        previous,
        false);
  }

  interaction.focused =
      tree.isValid(handle)
          ? handle
          : NodeHandle{};

  markStateChangeDirty(
      tree,
      previous,
      interaction.focused);

  if (tree.isValid(previous)) {
    events.push(
        UIEventType::FocusLost,
        previous);
  }

  if (tree.isValid(interaction.focused)) {
    events.push(
        UIEventType::FocusGained,
        interaction.focused);
  }
}

} // namespace octogui
