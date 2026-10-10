#include "Systems/InteractionSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/WidgetMetrics.h"

namespace octogui {

bool InteractionSystem::updateComboBoxPopupHover(
    Tree& tree,
    NodeHandle root,
    WidgetSystem& widgets,
    Vec2 mousePosition) {
  if (!tree.isValid(root) ||
      !tree.isValid(interaction.focused) ||
      tree.type(interaction.focused) !=
          NodeType::ComboBox) {
    return false;
  }

  const NodeHandle combo =
      interaction.focused;

  ComboBoxState* state =
      tree.comboBoxState(combo);

  if (!state ||
      !state->open ||
      state->items.empty()) {
    return false;
  }

  const Rect viewport =
      tree.rect(root);

  if (viewport.isEmpty()) {
    return false;
  }

    const Rect anchor =
      detail::visualRect(tree, combo);

  const Rect popup =
      detail::comboBoxPopupRect(
          anchor,
          viewport,
          state->items.size());

    const f32 maxScroll =
      detail::comboBoxMaxScrollY(
        popup,
        state->items.size());

    const f32 scrollY =
      state->popupScrollY < maxScroll
        ? state->popupScrollY
        : maxScroll;

    const Rect content =
      detail::comboBoxPopupContentRect(
        popup);

  if (!popup.contains(mousePosition)) {
    widgets.setComboBoxHoveredIndex(
        tree,
        combo,
        -1);

    return false;
  }

  if (!content.contains(mousePosition)) {
    widgets.setComboBoxHoveredIndex(
        tree,
        combo,
        -1);

    return true;
  }

  i32 hoveredIndex = -1;

  for (usize i = 0;
       i < state->items.size();
       ++i) {
    const Rect item =
        detail::comboBoxItemRect(
            popup,
            i,
          scrollY);

    if (item.maxY() <= content.y ||
        item.y >= content.maxY()) {
      continue;
    }

    if (item.contains(mousePosition)) {
      hoveredIndex =
          static_cast<i32>(i);

      break;
    }
  }

  widgets.setComboBoxHoveredIndex(
      tree,
      combo,
      hoveredIndex);

  return true;
}

} // namespace octogui
