#include "Systems/EventSystem.h"
#include "Systems/StyleSystem.h"
#include "Systems/WidgetSystem.h"

namespace octogui {

// Node creation (base)

NodeHandle WidgetSystem::createNode(Tree &tree, StyleSystem &styles,
                                    NodeType type, NodeHandle parent) {
  NodeHandle handle = tree.create(type, parent);

  if (type == NodeType::Button || type == NodeType::TextInput) {
    tree.setInteractive(handle, true);
    tree.setFocusable(handle, true);
  }

  styles.applyDefault(tree, handle, type);

  return handle;
}

// Generic interaction handling

void WidgetSystem::handleClick(Tree &tree, EventSystem &events,
                               NodeHandle handle) {
  if (!tree.isValid(handle) || !tree.isEnabled(handle)) {
    return;
  }

  const NodeType type = tree.type(handle);

  if (type == NodeType::Checkbox) {
    toggleChecked(tree, events, handle);
  } else if (type == NodeType::RadioButton) {
    selectRadioButton(tree, events, handle);
  } else if (type == NodeType::ComboBox) {
    ComboBoxState* state =
        tree.comboBoxState(handle);

    if (state) {
      setComboBoxOpen(
          tree,
          handle,
          !state->open);
    }
  }
  else if (type == NodeType::CollapsibleSection) {
    toggleCollapsibleSection(
        tree,
        handle);
  }
  
}

} // namespace octogui
