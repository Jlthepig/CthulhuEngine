#include "Systems/StyleSystem.h"
#include <vector>

namespace octogui {

namespace {

} // namespace

Theme &StyleSystem::theme() noexcept { return themeData; }

const Theme &StyleSystem::theme() const noexcept { return themeData; }

void StyleSystem::setTheme(Tree &tree, NodeHandle root, const Theme &theme) {
  std::vector<NodeHandle> defaultStyleNodes;

  if (tree.isValid(root)) {
    std::vector<NodeHandle> stack;
    stack.push_back(root);

    while (!stack.empty()) {
      const NodeHandle current = stack.back();
      stack.pop_back();

      const VisualStyle *style =
          static_cast<const Tree &>(tree).visualStyle(current);

      if (style && *style == defaultVisualStyle(tree.type(current))) {
        defaultStyleNodes.push_back(current);
      }

      NodeHandle child = tree.firstChild(current);

      while (tree.isValid(child)) {
        stack.push_back(child);
        child = tree.nextSibling(child);
      }
    }
  }

  themeData = theme;
  resetDefaults();

  for (NodeHandle handle : defaultStyleNodes) {
    if (!tree.isValid(handle)) {
      continue;
    }

    tree.setVisualStyle(handle, defaultVisualStyle(tree.type(handle)));
  }
}

VisualStyle &StyleSystem::defaultVisualStyle(NodeType type) noexcept {
  switch (type) {
  case NodeType::Root: {
    return defaultStyles.root;
  }
  case NodeType::Container: {
    return defaultStyles.container;
  }
  case NodeType::Button: {
    return defaultStyles.button;
  }
  case NodeType::CollapsibleSection: {
    return defaultStyles.button;
  }
  case NodeType::Label: {
    return defaultStyles.label;
  }
  case NodeType::Checkbox: {
    return defaultStyles.checkbox;
  }
  case NodeType::Slider:
  case NodeType::ProgressBar: {
    return defaultStyles.slider;
  }
  case NodeType::RadioButton: {
    return defaultStyles.radioButton;
  }
  case NodeType::TextInput: {
    return defaultStyles.textInput;
  }
  case NodeType::NumericInput: {
    return defaultStyles.textInput;
  }
  case NodeType::ComboBox: {
    return defaultStyles.textInput;
  }
  case NodeType::TreeView: {
    return defaultStyles.container;
  }
  case NodeType::Separator: {
    return defaultStyles.separator;
  }
  case NodeType::SplitContainer: {
    return defaultStyles.separator;
  }
  case NodeType::Icon: {
    return defaultStyles.custom;
  }
  case NodeType::Custom: {
    return defaultStyles.custom;
  }
  }

  return defaultStyles.custom;
}

const VisualStyle &
StyleSystem::defaultVisualStyle(NodeType type) const noexcept {
  switch (type) {
  case NodeType::Root: {
    return defaultStyles.root;
  }
  case NodeType::Container: {
    return defaultStyles.container;
  }
  case NodeType::Button: {
    return defaultStyles.button;
  }
  case NodeType::CollapsibleSection: {
    return defaultStyles.button;
  }
  case NodeType::Label: {
    return defaultStyles.label;
  }
  case NodeType::Checkbox: {
    return defaultStyles.checkbox;
  }
  case NodeType::Slider:
  case NodeType::ProgressBar: {
    return defaultStyles.slider;
  }
  case NodeType::RadioButton: {
    return defaultStyles.radioButton;
  }
  case NodeType::TextInput: {
    return defaultStyles.textInput;
  }
  case NodeType::NumericInput: {
    return defaultStyles.textInput;
  }
  case octogui::NodeType::ComboBox: {
    return defaultStyles.textInput;
  }
  case NodeType::TreeView: {
    return defaultStyles.container;
  }
  case NodeType::Separator: {
    return defaultStyles.separator;
  }
  case NodeType::SplitContainer: {
    return defaultStyles.separator;
  }
  case NodeType::Icon: {
    return defaultStyles.custom;
  }
  case NodeType::Custom: {
    return defaultStyles.custom;
  }
  }

  return defaultStyles.custom;
}

void StyleSystem::resetDefaults() noexcept {
  defaultStyles.root = makeRootStyle(themeData);
  defaultStyles.container = makePanelStyle(themeData);
  defaultStyles.button = makeButtonStyle(themeData);
  defaultStyles.label = makeLabelStyle(themeData);
  defaultStyles.checkbox = makeCheckboxStyle(themeData);
  defaultStyles.slider = makeSliderStyle(themeData);
  defaultStyles.radioButton = makeRadioButtonStyle(themeData);
  defaultStyles.textInput = makeTextInputStyle(themeData);
  defaultStyles.separator = makeSeparatorStyle(themeData);
  defaultStyles.custom = VisualStyle{};
}

void StyleSystem::applyDefault(Tree &tree, NodeHandle handle, NodeType type) {
  if (!tree.isValid(handle)) {
    return;
  }

  tree.setVisualStyle(handle, defaultVisualStyle(type));
}

} // namespace octogui