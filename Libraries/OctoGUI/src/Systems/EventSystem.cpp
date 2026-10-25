#include "Systems/EventSystem.h"

namespace octogui {

void EventSystem::push(UIEventType type, NodeHandle target) {
  UIEvent event;
  event.type = type;
  event.target = target;

  queue.push_back(event);
}

void EventSystem::pushPointer(UIEventType type, NodeHandle target,
                              Vec2 position) {
  UIEvent event;
  event.type = type;
  event.target = target;
  event.position = position;

  queue.push_back(event);
}

void EventSystem::pushValueChanged(NodeHandle target, f64 oldValue,
                                   f64 newValue) {
  UIEvent event;
  event.type = UIEventType::ValueChanged;
  event.target = target;
  event.oldValue = oldValue;
  event.newValue = newValue;

  queue.push_back(event);
}

void EventSystem::pushCheckedChanged(NodeHandle target, bool oldChecked,
                                     bool newChecked) {
  UIEvent event;
  event.type = UIEventType::CheckedChanged;
  event.target = target;
  event.oldChecked = oldChecked;
  event.newChecked = newChecked;

  queue.push_back(event);
}

void EventSystem::pushSelectionChanged(NodeHandle target, TreeViewItemId oldSelection, TreeViewItemId newSelection) {
  UIEvent event;
  event.type =UIEventType::SelectionChanged;
  event.target = target;
  event.oldSelection = oldSelection;
  event.newSelection = newSelection;

  queue.push_back(event);
}

void EventSystem::pushTreeViewDrop(NodeHandle target, TreeViewItemId dragged, TreeViewItemId dropTarget, TreeViewDropPosition position) {
  UIEvent event;
  event.type = UIEventType::TreeViewDrop;
  event.target = target;
  event.draggedItem = dragged;
  event.dropTarget = dropTarget;
  event.dropPosition = position;
  queue.push_back(event);
}

bool EventSystem::pollFiltered(UIEvent &event, UIEventType type, NodeHandle target) noexcept {
  for (usize index = readIndex; index < queue.size(); ++index) {
    if (queue[index].type != type) {
      continue;
    }
    if (target && queue[index].target != target) {
      continue;
    }

    event = queue[index];
    queue.erase(queue.begin() + index);
    if (readIndex > queue.size()) {
      readIndex = queue.size();
    }
    if (readIndex == queue.size()) {
      queue.clear();
      readIndex = 0;
    }
    return true;
  }
  return false;
}

bool EventSystem::poll(UIEvent &event) noexcept {
  if (readIndex >= queue.size()) {
    return false;
  }

  event = queue[readIndex++];

  if (readIndex == queue.size()) {
    queue.clear();
    readIndex = 0;
  }

  return true;
}

bool EventSystem::empty() const noexcept { return readIndex >= queue.size(); }

usize EventSystem::size() const noexcept { return queue.size() - readIndex; }

void EventSystem::clear() noexcept {
  queue.clear();
  readIndex = 0;
}

} // namespace octogui