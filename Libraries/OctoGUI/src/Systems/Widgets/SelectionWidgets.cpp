#include <cmath>

#include "Systems/EventSystem.h"
#include "Systems/StyleSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/WidgetMetrics.h"

namespace octogui {

// Checkbox / RadioButton and shared checked state

NodeHandle WidgetSystem::createCheckbox(Tree &tree, StyleSystem &styles,
                                        NodeHandle parent,
                                        std::string_view text, bool checked) {
  NodeHandle handle = createNode(tree, styles, NodeType::Checkbox, parent);

  tree.setInteractive(handle, true);
  tree.setFocusable(handle, true);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize.y = 24.0f;
    layout->minSize.y = 18.0f;
  }

  if (!text.empty()) {
    tree.setText(handle, text);
  }
  if (checked) {
    tree.setChecked(handle, true);
  }

  return handle;
}

NodeHandle WidgetSystem::createRadioButton(Tree &tree, StyleSystem &styles,
                                           NodeHandle parent,
                                           std::string_view text,
                                           bool checked) {
  NodeHandle handle = createNode(tree, styles, NodeType::RadioButton, parent);

  tree.setInteractive(handle, true);
  tree.setFocusable(handle, true);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize.y = 24.0f;
    layout->minSize.y = 18.0f;
  }

  if (!text.empty()) {
    tree.setText(handle, text);
  }
  if (checked) {
    tree.setChecked(handle, true);
  }

  return handle;
}

bool WidgetSystem::isChecked(const Tree &tree,
                             NodeHandle handle) const noexcept {
  return tree.isValid(handle) && hasFlag(tree.flags(handle), NodeFlags::Checked);
}

void WidgetSystem::setChecked(Tree &tree, EventSystem &events,
                              NodeHandle handle, bool checked) {
  if (!tree.isValid(handle)) {
    return;
  }

  if (tree.type(handle) == NodeType::RadioButton) {
    if (checked) {
      selectRadioButton(tree, events, handle);
    } else {
      const bool oldChecked = isChecked(tree, handle);

      if (!oldChecked) {
        return;
      }

      tree.setChecked(handle, false);
      tree.markPaintDirty(handle);
      events.pushCheckedChanged(handle, true, false);
    }

    return;
  }

  const bool oldChecked = isChecked(tree, handle);

  if (oldChecked == checked) {
    return;
  }

  if (checked) {
    tree.setChecked(handle, true);
  } else {
    tree.setChecked(handle, false);
  }

  tree.markPaintDirty(handle);
  events.pushCheckedChanged(handle, oldChecked, checked);
}

void WidgetSystem::toggleChecked(Tree &tree, EventSystem &events,
                                 NodeHandle handle) {
  if (!tree.isValid(handle)) {
    return;
  }

  if (tree.type(handle) == NodeType::RadioButton) {
    selectRadioButton(tree, events, handle);
    return;
  }

  setChecked(tree, events, handle, !isChecked(tree, handle));
}

void WidgetSystem::selectRadioButton(Tree &tree, EventSystem &events,
                                     NodeHandle handle) {
  if (!tree.isValid(handle)) {
    return;
  }
  if (tree.type(handle) != NodeType::RadioButton) {
    return;
  }

  const NodeHandle parent = tree.parent(handle);

  if (tree.isValid(parent)) {
    NodeHandle sibling = tree.firstChild(parent);

    while (tree.isValid(sibling)) {
      if (sibling != handle && tree.type(sibling) == NodeType::RadioButton &&
          isChecked(tree, sibling)) {
        tree.setChecked(sibling, false);
        tree.markPaintDirty(sibling);
        events.pushCheckedChanged(sibling, true, false);
      }

      sibling = tree.nextSibling(sibling);
    }
  }

  if (!isChecked(tree, handle)) {
    tree.setChecked(handle, true);
    tree.markPaintDirty(handle);
    events.pushCheckedChanged(handle, false, true);
  }
}

// Slider

NodeHandle WidgetSystem::createSlider(Tree &tree, StyleSystem &styles,
                                      NodeHandle parent, f32 value,
                                      f32 minValue, f32 maxValue) {
  NodeHandle handle = createNode(tree, styles, NodeType::Slider, parent);

  tree.setInteractive(handle, true);
  tree.setFocusable(handle, true);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize.y = 20.0f;
    layout->minSize.y = 16.0f;
  }

  SliderState *state = tree.sliderState(handle);

  if (state) {
    if (maxValue < minValue) {
      maxValue = minValue;
    }

    state->minValue = minValue;
    state->maxValue = maxValue;
    state->step = 0.0f;
    state->value = clampSliderValue(*state, value);
  }

  return handle;
}

f32 WidgetSystem::sliderValue(const Tree &tree,
                              NodeHandle handle) const noexcept {
  const SliderState *state = tree.sliderState(handle);

  if (!state) {
    return 0.0f;
  }

  return state->value;
}

void WidgetSystem::setSliderValue(Tree &tree, EventSystem &events,
                                  NodeHandle handle, f32 value) {
  SliderState *state = tree.sliderState(handle);

  if (!state) {
    return;
  }

  const f32 oldValue = state->value;
  const f32 newValue = clampSliderValue(*state, value);

  if (oldValue == newValue) {
    return;
  }

  state->value = newValue;
  tree.markPaintDirty(handle);
  events.pushValueChanged(handle, oldValue, newValue);
}

void WidgetSystem::setSliderRange(Tree &tree, EventSystem &events,
                                  NodeHandle handle, f32 minValue,
                                  f32 maxValue) {
  SliderState *state = tree.sliderState(handle);

  if (!state) {
    return;
  }

  if (maxValue < minValue) {
    maxValue = minValue;
  }

  const f32 oldValue = state->value;
  const f32 oldMin = state->minValue;
  const f32 oldMax = state->maxValue;

  state->minValue = minValue;
  state->maxValue = maxValue;
  state->value = clampSliderValue(*state, state->value);

  if (oldMin != state->minValue || oldMax != state->maxValue ||
      oldValue != state->value) {
    tree.markPaintDirty(handle);
  }

  if (oldValue != state->value) {
    events.pushValueChanged(handle, oldValue, state->value);
  }
}

void WidgetSystem::setSliderStep(Tree &tree, EventSystem &events,
                                 NodeHandle handle, f32 step) {
  SliderState *state = tree.sliderState(handle);

  if (!state) {
    return;
  }

  if (step < 0.0f) {
    step = 0.0f;
  }

  const f32 oldStep = state->step;
  const f32 oldValue = state->value;

  state->step = step;
  state->value = clampSliderValue(*state, state->value);

  if (oldStep != state->step || oldValue != state->value) {
    tree.markPaintDirty(handle);
  }

  if (oldValue != state->value) {
    events.pushValueChanged(handle, oldValue, state->value);
  }
}

void WidgetSystem::updateSliderFromMouse(Tree &tree, EventSystem &events,
                                         NodeHandle handle,
                                         Vec2 mousePosition) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::Slider) {
    return;
  }

  SliderState *state = tree.sliderState(handle);

  if (!state) {
    return;
  }

  const Rect content =
      detail::contentRect(tree, handle, tree.rect(handle));

  if (content.isEmpty()) {
    return;
  }

  const f32 handleRadius = detail::sliderHandleRadiusForContent(content);
  const f32 usableWidth = content.w - handleRadius * 2.0f;

  f32 t = 0.0f;

  if (usableWidth > 0.0f) {
    t = (mousePosition.x - (content.x + handleRadius)) / usableWidth;

    if (t < 0.0f) {
      t = 0.0f;
    }
    if (t > 1.0f) {
      t = 1.0f;
    }
  }

  f32 minValue = state->minValue;
  f32 maxValue = state->maxValue;

  if (maxValue < minValue) {
    maxValue = minValue;
  }

  const f32 value = minValue + t * (maxValue - minValue);
  const f32 newValue = clampSliderValue(*state, value);

  const f32 oldValue = state->value;

  if (oldValue == newValue) {
    return;
  }

  state->value = newValue;
  tree.markPaintDirty(handle);
  events.pushValueChanged(handle, oldValue, newValue);
}

f32 WidgetSystem::clampSliderValue(const SliderState &state,
                                   f32 value) const noexcept {
  f32 minValue = state.minValue;
  f32 maxValue = state.maxValue;

  if (maxValue < minValue) {
    maxValue = minValue;
  }

  if (value < minValue) {
    value = minValue;
  }
  if (value > maxValue) {
    value = maxValue;
  }

  if (state.step > 0.0f) {
    value = minValue + std::round((value - minValue) / state.step) * state.step;

    if (value < minValue) {
      value = minValue;
    }
    if (value > maxValue) {
      value = maxValue;
    }
  }

  return value;
}

  // combo box

  NodeHandle WidgetSystem::createComboBox(
    Tree& tree,
    StyleSystem& styles,
    NodeHandle parent,
    const std::vector<std::string>& items,
    i32 selectedIndex) {
  NodeHandle handle =
      createNode(tree, styles, NodeType::ComboBox, parent);

  tree.setInteractive(handle, true);
  tree.setFocusable(handle, true);

  tree.setTextAlign(
      handle,
      TextAlign::Start,
      TextAlign::Center);

  LayoutStyle* layout =
      tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize =
        Vec2{180.0f, 34.0f};

    layout->minSize =
        Vec2{80.0f, 28.0f};

    layout->padding =
        Padding{10.0f, 6.0f};
  }

  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state) {
    return handle;
  }

  state->items = items;

  if (items.empty()) {
    state->selectedIndex = -1;
  } else if (selectedIndex < 0) {
    state->selectedIndex = -1;
  } else if (selectedIndex >=
             static_cast<i32>(items.size())) {
    state->selectedIndex =
        static_cast<i32>(items.size()) - 1;
  } else {
    state->selectedIndex =
        selectedIndex;
  }

  return handle;
}

i32 WidgetSystem::comboBoxSelectedIndex(
    const Tree& tree,
    NodeHandle handle) const noexcept {
  const ComboBoxState* state =
      tree.comboBoxState(handle);

  return state
      ? state->selectedIndex
      : -1;
}

std::string_view WidgetSystem::comboBoxSelectedText(
    const Tree& tree,
    NodeHandle handle) const noexcept {
  const ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state ||
      state->selectedIndex < 0 ||
      state->selectedIndex >=
          static_cast<i32>(state->items.size())) {
    return {};
  }

  return state->items[
      static_cast<usize>(state->selectedIndex)];
}

void WidgetSystem::setComboBoxSelectedIndex(
    Tree& tree,
    EventSystem& events,
    NodeHandle handle,
    i32 index) {
  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state) {
    return;
  }

  if (state->items.empty()) {
    index = -1;
  } else {
    if (index < -1) {
      index = -1;
    }

    const i32 lastIndex =
        static_cast<i32>(state->items.size()) - 1;

    if (index > lastIndex) {
      index = lastIndex;
    }
  }

  if (state->selectedIndex == index) {
    return;
  }

  const i32 oldIndex =
      state->selectedIndex;

  state->selectedIndex = index;

  tree.markPaintDirty(handle);

  events.pushValueChanged(
      handle,
      static_cast<f64>(oldIndex),
      static_cast<f64>(index));
}

void WidgetSystem::setComboBoxItems(
    Tree& tree,
    EventSystem& events,
    NodeHandle handle,
    const std::vector<std::string>& items) {
  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state) {
    return;
  }

  const i32 oldIndex =
      state->selectedIndex;

  state->items = items;
  state->hoveredIndex = -1;
  state->open = false;
  state->popupScrollY = 0.0f;

  if (state->items.empty()) {
    state->selectedIndex = -1;
  } else if (state->selectedIndex >=
             static_cast<i32>(state->items.size())) {
    state->selectedIndex =
        static_cast<i32>(state->items.size()) - 1;
  }

  tree.markPaintDirty(handle);

  if (oldIndex != state->selectedIndex) {
    events.pushValueChanged(
        handle,
        static_cast<f64>(oldIndex),
        static_cast<f64>(state->selectedIndex));
  }
}

void WidgetSystem::setComboBoxOpen(
    Tree& tree,
    NodeHandle handle,
    bool open) {
  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state) {
    return;
  }

  if (state->items.empty()) {
    open = false;
  }

  if (state->open == open) {
    return;
  }

  state->open = open;
  state->hoveredIndex = -1;

  if (open) {
    if (state->selectedIndex >= 0) {
      const i32 halfVisible =
          static_cast<i32>(
              detail::ComboBoxMaxVisibleItems / 2);

      i32 firstIndex =
          state->selectedIndex -
          halfVisible;

      if (firstIndex < 0) {
        firstIndex = 0;
      }

      state->popupScrollY =
          static_cast<f32>(firstIndex) *
          detail::ComboBoxItemHeight;
    } else {
      state->popupScrollY = 0.0f;
    }
  } else {
    state->popupScrollY = 0.0f;
  }

  tree.markPaintDirty(handle);
}

bool WidgetSystem::isComboBoxOpen(
    const Tree& tree,
    NodeHandle handle) const noexcept {
  const ComboBoxState* state =
      tree.comboBoxState(handle);

  return state && state->open;
}

void WidgetSystem::setComboBoxHoveredIndex(
    Tree& tree,
    NodeHandle handle,
    i32 index) {
  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state) {
    return;
  }

  if (index < -1) {
    index = -1;
  }

  if (index >=
      static_cast<i32>(state->items.size())) {
    index = -1;
  }

  if (state->hoveredIndex == index) {
    return;
  }

  state->hoveredIndex = index;
}

bool WidgetSystem::selectComboBoxHovered(
    Tree& tree,
    EventSystem& events,
    NodeHandle handle) {
  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state ||
      !state->open ||
      state->hoveredIndex < 0 ||
      state->hoveredIndex >=
          static_cast<i32>(state->items.size())) {
    return false;
  }

  const i32 index =
      state->hoveredIndex;

  setComboBoxSelectedIndex(
      tree,
      events,
      handle,
      index);

  setComboBoxOpen(
      tree,
      handle,
      false);

  return true;
}

void WidgetSystem::setComboBoxScrollY(
    Tree& tree,
    NodeHandle handle,
    f32 scrollY,
    f32 maxScrollY) {
  ComboBoxState* state =
      tree.comboBoxState(handle);

  if (!state) {
    return;
  }

  if (maxScrollY < 0.0f) {
    maxScrollY = 0.0f;
  }

  if (scrollY < 0.0f) {
    scrollY = 0.0f;
  }

  if (scrollY > maxScrollY) {
    scrollY = maxScrollY;
  }

  state->popupScrollY = scrollY;
}

} // namespace octogui
