#pragma once

#include <vector>

#include "OctoGui/Event.h"

namespace octogui {
class EventSystem {
public:
  void push(UIEventType type, NodeHandle target);
  void pushPointer(UIEventType type, NodeHandle target, Vec2 position);
  void pushValueChanged(NodeHandle target, f64 oldValue, f64 newValue);
  void pushCheckedChanged(NodeHandle target, bool oldChecked, bool newChecked);
  void pushSelectionChanged(NodeHandle target, TreeViewItemId oldSelection, TreeViewItemId newSelection);

  [[nodiscard]]
  bool poll(UIEvent &event) noexcept;

  [[nodiscard]]
  bool empty() const noexcept;

  [[nodiscard]]
  usize size() const noexcept;

  void clear() noexcept;

private:
  std::vector<UIEvent> queue;
  usize readIndex = 0;
};
} // namespace octogui