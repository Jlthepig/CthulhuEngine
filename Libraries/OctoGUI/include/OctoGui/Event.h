#pragma once

#include "OctoGui/Node.h"
#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {

enum class UIEventType {
  None,

  PointerEnter,
  PointerLeave,
  PointerPressed,
  PointerReleased,
  Clicked,

  FocusGained,
  FocusLost,

  ValueChanged,
  CheckedChanged,
  SelectionChanged
};

struct UIEvent {
  UIEventType type = UIEventType::None;
  NodeHandle target{};

  Vec2 position{};

  f64 oldValue = 0.0f;
  f64 newValue = 0.0f;

  bool oldChecked = false;
  bool newChecked = false;

  TreeViewItemId oldSelection{};
  TreeViewItemId newSelection{};
};

} // namespace octogui