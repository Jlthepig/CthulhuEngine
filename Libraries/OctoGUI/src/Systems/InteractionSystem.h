#pragma once

#include <vector>

#include "OctoGui/Font.h"
#include "OctoGui/Input.h"
#include "OctoGui/Tree.h"

namespace octogui {

class WidgetSystem;
class EventSystem;

struct InteractionState {
  NodeHandle hovered{};
  NodeHandle active{};
  NodeHandle focused{};
  NodeHandle pressed{};
  NodeHandle released{};
  NodeHandle clicked{};
  NodeHandle scrollbarHovered{};
  NodeHandle scrollbarActive{};
  NodeHandle splitterHovered{};
  NodeHandle splitterActive{};
};

class InteractionSystem {
public:
  void update(Tree &tree, NodeHandle root, const InputState &input,
              WidgetSystem &widgets, EventSystem &events, Font *font);

  [[nodiscard]]
  const InteractionState &state() const noexcept;

  void setFocused(Tree& tree, WidgetSystem& widgets, EventSystem& events, NodeHandle handle);

  void clearFocus(Tree& tree, WidgetSystem& widgets, EventSystem& events);

  void onNodeDestroyed(Tree &tree, EventSystem &events,
                       NodeHandle handle) noexcept;

private:
  InteractionState interaction;

  std::vector<NodeHandle> zOrderScratch;
  f32 scrollbarGrabOffsetY = 0.0f;
  f32 splitterGrabOffset = 0.0f;

  [[nodiscard]]
  NodeHandle hitTest(const Tree &tree, NodeHandle node, Vec2 point,
                     Vec2 offset);

  bool applyWheelScroll(Tree &tree, NodeHandle hit, f32 wheelY);

  [[nodiscard]]
  bool isEnabledBranch(const Tree &tree, NodeHandle node) const;

  void changeFocus(Tree& tree, WidgetSystem& widgets, EventSystem& events, NodeHandle handle);

  [[nodiscard]]
  NodeHandle findInteractive(const Tree &tree, NodeHandle node) const;

  void markStateChangeDirty(Tree &tree, NodeHandle previous,
                            NodeHandle current);

  [[nodiscard]]
  NodeHandle hitTestScrollbar(const Tree &tree, NodeHandle node, Vec2 point,
                              Vec2 offset);

  [[nodiscard]]
  NodeHandle hitTestSplitter(const Tree &tree, NodeHandle node, Vec2 point,
                             Vec2 offset);

  void resetTransientState() noexcept;
  void validateTrackedNodes(Tree& tree, WidgetSystem& widgets, EventSystem& events);

  void updatePointerTargets(Tree& tree, NodeHandle root, const InputState& input, WidgetSystem& widgets, EventSystem& events);

  void handleLeftPress(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font);
  void updateActivePointerDrag(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font);
  void handleLeftRelease(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font);

  void updateFocusedEditing(Tree& tree, const InputState& input, WidgetSystem& widgets, EventSystem& events, Font* font);

  void markInteractionChanges(Tree& tree, const InteractionState& previous);

  void beginScrollbarInteraction(Tree &tree, NodeHandle node,
                                 Vec2 mousePosition);

  void updateScrollbarDrag(Tree &tree, NodeHandle node, Vec2 mousePosition);

  void beginSplitterInteraction(Tree &tree, NodeHandle node,
                                Vec2 mousePosition);

  void updateSplitterDrag(Tree &tree, WidgetSystem &widgets, NodeHandle node,
                          Vec2 mousePosition);

  [[nodiscard]]
  bool updateComboBoxPopupHover(
      Tree& tree,
      NodeHandle root,
      WidgetSystem& widgets,
      Vec2 mousePosition);

};

} // namespace octogui