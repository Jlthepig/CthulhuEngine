#include "OctoGui/Tree.h"
#include <utility>

namespace octogui {

namespace {

template <typename State>
State& addSparseState(u32 nodeIndex, std::vector<State>& states,
                      std::vector<u32>& owners, std::vector<u32>& indices) {
  const u32 stateIndex = static_cast<u32>(states.size());

  states.emplace_back();
  owners.push_back(nodeIndex);
  indices[nodeIndex] = stateIndex;

  return states.back();
}

template <typename State>
void removeSparseState(u32 nodeIndex, u32 invalidIndex,
                       std::vector<State>& states,
                       std::vector<u32>& owners,
                       std::vector<u32>& indices) noexcept {
  const u32 stateIndex = indices[nodeIndex];

  if (stateIndex == invalidIndex) {return;}

  const u32 lastIndex = static_cast<u32>(states.size() - 1);

  if (stateIndex != lastIndex) {
    states[stateIndex] = std::move(states[lastIndex]);

    const u32 movedOwner = owners[lastIndex];

    owners[stateIndex] = movedOwner;
    indices[movedOwner] = stateIndex;
  }

  states.pop_back();
  owners.pop_back();

  indices[nodeIndex] = invalidIndex;
}

} // namespace

NodeHandle Tree::create(NodeType type, NodeHandle parent) {
  const u32 index = allocateSlot(type);
  NodeHandle handle{index, generations[index]};

  if (isValid(parent)) {
    linkChild(handle, parent);
  }

  return handle;
}

NodeHandle Tree::createRoot(NodeType type) {
  return create(type, NodeHandle{});
}

void Tree::markLayoutDirty(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return;
  }

  nodeFlags[handle.index] |= NodeFlags::LayoutDirty;

  NodeHandle current = parents[handle.index];

  while (isValid(current)) {
    nodeFlags[current.index] |= NodeFlags::ChildrenDirty;
    current = parents[current.index];
  }
}

void Tree::clearLayoutDirty(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return;
  }

    nodeFlags[handle.index] = removeFlags(
      nodeFlags[handle.index], NodeFlags::LayoutDirty | NodeFlags::ChildrenDirty);
}

void Tree::markPaintDirty(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return;
  }

  nodeFlags[handle.index] |= NodeFlags::PaintDirty;

  NodeHandle current = parents[handle.index];

  while (isValid(current)) {
    nodeFlags[current.index] |= NodeFlags::PaintChildrenDirty;
    current = parents[current.index];
  }
}

void Tree::clearPaintDirty(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return;
  }

    nodeFlags[handle.index] =
      removeFlags(nodeFlags[handle.index],
                  NodeFlags::PaintDirty | NodeFlags::PaintChildrenDirty);
}

void Tree::markStyleDirty(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return;
  }

  nodeFlags[handle.index] |= NodeFlags::StyleDirty;

  markPaintDirty(handle);
}

void Tree::markStyleDirtySubtree(NodeHandle handle) {
  if (!isValid(handle)) {
    return;
  }

  std::vector<NodeHandle> stack;
  stack.reserve(32);
  stack.push_back(handle);

  while (!stack.empty()) {
    NodeHandle current = stack.back();
    stack.pop_back();

    if (!isValid(current)) {
      continue;
    }

    nodeFlags[current.index] |= NodeFlags::StyleDirty | NodeFlags::PaintDirty;

    NodeHandle child = firstChildren[current.index];

    while (isValid(child)) {
      stack.push_back(child);
      child = nextSiblings[child.index];
    }
  }

  NodeHandle current = parents[handle.index];

  while (isValid(current)) {
    nodeFlags[current.index] |= NodeFlags::PaintChildrenDirty;
    current = parents[current.index];
  }
}

void Tree::clearStyleDirty(NodeHandle handle) noexcept {
  if (!isValid(handle)) {
    return;
  }

  nodeFlags[handle.index] = removeFlags(nodeFlags[handle.index], NodeFlags::StyleDirty);
}

void Tree::destroy(NodeHandle handle) {
  if (!isValid(handle)) {
    return;
  }

  unlink(handle);

  std::vector<NodeHandle> stack;
  stack.reserve(32);
  stack.push_back(handle);

  while (!stack.empty()) {
    NodeHandle current = stack.back();
    stack.pop_back();

    if (!isValid(current)) {
      continue;
    }

    NodeHandle child = firstChildren[current.index];

    while (isValid(child)) {
      const NodeHandle next = nextSiblings[child.index];
      stack.push_back(child);
      child = next;
    }

    freeSlot(current.index);
  }
}

void Tree::setParent(NodeHandle handle, NodeHandle newParent) {
  if (!isValid(handle)) {
    return;
  }

  if (handle == newParent) {
    return;
  }

  if (isValid(newParent) && isDescendant(handle, newParent)) {
    return;
  }

  unlink(handle);

  if (isValid(newParent)) {
    linkChild(handle, newParent);
  }
}

bool Tree::isDescendant(NodeHandle ancestor,
                        NodeHandle descendant) const noexcept {
  if (!isValid(ancestor)) {
    return false;
  }

  NodeHandle current = descendant;

  while (isValid(current)) {
    if (current == ancestor) {
      return true;
    }

    current = parents[current.index];
  }

  return false;
}

void Tree::reserve(usize capacity) {
  generations.reserve(capacity);
  alive.reserve(capacity);
  types.reserve(capacity);
  nodeFlags.reserve(capacity);
  parents.reserve(capacity);
  firstChildren.reserve(capacity);
  lastChildren.reserve(capacity);
  prevSiblings.reserve(capacity);
  nextSiblings.reserve(capacity);
  childCounts.reserve(capacity);
  rects.reserve(capacity);
  scrollOffsetsY.reserve(capacity);
  layoutStyles.reserve(capacity);
  styleHandles.reserve(capacity);
  layoutResults.reserve(capacity);
  freeList.reserve(capacity);
  textStates.reserve(capacity);
  textOwners.reserve(capacity);
  textStateIndices.reserve(capacity);
  sliderStates.reserve(capacity);
  sliderOwners.reserve(capacity);
  sliderStateIndices.reserve(capacity);
  textInputStates.reserve(capacity);
  textInputOwners.reserve(capacity);
  textInputStateIndices.reserve(capacity);
  separatorStates.reserve(capacity);
  separatorOwners.reserve(capacity);
  separatorStateIndices.reserve(capacity);
  splitContainerStates.reserve(capacity);
  splitContainerOwners.reserve(capacity);
  splitContainerStateIndices.reserve(capacity);
  numericInputStates.reserve(capacity);
  numericInputOwners.reserve(capacity);
  numericInputStateIndices.reserve(capacity);
  comboBoxStates.reserve(capacity);
  comboBoxOwners.reserve(capacity);
  comboBoxStateIndices.reserve(capacity);
  collapsibleSectionStates.reserve(capacity);
  collapsibleSectionOwners.reserve(capacity);
  collapsibleSectionStateIndices.reserve(capacity);
  progressBarStates.reserve(capacity);
  progressBarOwners.reserve(capacity);
  progressBarStateIndices.reserve(capacity);
  treeViewStates.reserve(capacity);
  treeViewOwners.reserve(capacity);
  treeViewStateIndices.reserve(capacity);
  iconStates.reserve(capacity);
  iconOwners.reserve(capacity);
  iconStateIndices.reserve(capacity);
  zLayers.reserve(capacity);
}

void Tree::clear() noexcept {
  generations.clear();
  alive.clear();
  types.clear();
  nodeFlags.clear();
  parents.clear();
  firstChildren.clear();
  lastChildren.clear();
  prevSiblings.clear();
  nextSiblings.clear();
  childCounts.clear();
  rects.clear();
  scrollOffsetsY.clear();
  layoutStyles.clear();
  styleHandles.clear();
  styleTable.clear();
  freeStyleSlots.clear();
  layoutResults.clear();
  freeList.clear();
  textStates.clear();
  textOwners.clear();
  textStateIndices.clear();
  sliderStates.clear();
  sliderOwners.clear();
  sliderStateIndices.clear();
  textInputStates.clear();
  textInputOwners.clear();
  textInputStateIndices.clear();
  separatorStates.clear();
  separatorOwners.clear();
  separatorStateIndices.clear();
  splitContainerStates.clear();
  splitContainerOwners.clear();
  splitContainerStateIndices.clear();
  numericInputStates.clear();
  numericInputOwners.clear();
  numericInputStateIndices.clear();
  comboBoxStates.clear();
  comboBoxOwners.clear();
  comboBoxStateIndices.clear();
  collapsibleSectionStates.clear();
  collapsibleSectionOwners.clear();
  collapsibleSectionStateIndices.clear();
  progressBarStates.clear();
  progressBarOwners.clear();
  progressBarStateIndices.clear();
  treeViewStates.clear();
  treeViewOwners.clear();
  treeViewStateIndices.clear();
  iconStates.clear();
  iconOwners.clear();
  iconStateIndices.clear();
  zLayers.clear();
  aliveCount = 0;
}

Tree::StyleHandle Tree::createUniqueStyle(const VisualStyle &style) {
  if (!freeStyleSlots.empty()) {
    const u32 index = freeStyleSlots.back();
    freeStyleSlots.pop_back();

    styleTable[index].style = style;
    styleTable[index].refCount = 1;

    return StyleHandle{index};
  }

  const u32 index = static_cast<u32>(styleTable.size());

  styleTable.push_back(StyleEntry{.style = style, .refCount = 1});

  return StyleHandle{index};
}

Tree::StyleHandle Tree::acquireStyle(const VisualStyle &style) {
  for (u32 i = 0; i < static_cast<u32>(styleTable.size()); ++i) {
    StyleEntry &entry = styleTable[i];

    if (entry.refCount == 0) {
      continue;
    }

    if (entry.style == style) {
      ++entry.refCount;
      return StyleHandle{i};
    }
  }

  return createUniqueStyle(style);
}

void Tree::releaseStyle(Tree::StyleHandle handle) noexcept {
  if (!handle.isValid() || handle.index >= styleTable.size()) {
    return;
  }

  StyleEntry &entry = styleTable[handle.index];

  if (entry.refCount == 0) {
    return;
  }

  --entry.refCount;

  if (entry.refCount == 0) {
    freeStyleSlots.push_back(handle.index);
  }
}

void Tree::ensureUniqueStyle(NodeHandle handle) {
  if (!isValid(handle)) {
    return;
  }

  const StyleHandle current = styleHandles[handle.index];

  if (!current.isValid() || current.index >= styleTable.size()) {
    return;
  }

  if (styleTable[current.index].refCount <= 1) {
    return;
  }

  const VisualStyle copy = styleTable[current.index].style;

  --styleTable[current.index].refCount;

  styleHandles[handle.index] = createUniqueStyle(copy);
}

u32 Tree::allocateSlot(NodeType type) {
  u32 index;

  if (!freeList.empty()) {
    index = freeList.back();
    freeList.pop_back();
  } else {
    index = static_cast<u32>(generations.size());

    generations.push_back(1);
    alive.push_back(0);
    types.push_back(type);
    nodeFlags.push_back(NodeFlags::None);
    parents.push_back(NodeHandle{});
    firstChildren.push_back(NodeHandle{});
    lastChildren.push_back(NodeHandle{});
    prevSiblings.push_back(NodeHandle{});
    nextSiblings.push_back(NodeHandle{});
    childCounts.push_back(0);
    rects.push_back(Rect::zero());
    scrollOffsetsY.push_back(0.0f);
    textStateIndices.push_back(InvalidIndex);
    sliderStateIndices.push_back(InvalidIndex);
    textInputStateIndices.push_back(InvalidIndex);
    separatorStateIndices.push_back(InvalidIndex);
    splitContainerStateIndices.push_back(InvalidIndex);
    numericInputStateIndices.push_back(InvalidIndex);
    comboBoxStateIndices.push_back(InvalidIndex);
    collapsibleSectionStateIndices.push_back(InvalidIndex);
    progressBarStateIndices.push_back(InvalidIndex);
    treeViewStateIndices.push_back(InvalidIndex);
    iconStateIndices.push_back(InvalidIndex);
    layoutStyles.push_back(LayoutStyle{});
    styleHandles.push_back(StyleHandle{});
    layoutResults.push_back(LayoutResult{});
    zLayers.push_back(0);
  }

  initializeSlot(index, type);

  return index;
}

void Tree::initializeSlot(u32 index, NodeType type) {
  alive[index] = 1;
  types[index] = type;
  nodeFlags[index] = NodeFlags::Visible | NodeFlags::Enabled |
                 NodeFlags::LayoutDirty | NodeFlags::PaintDirty |
                 NodeFlags::StyleDirty;
  zLayers[index] = 0;

  parents[index] = NodeHandle{};
  firstChildren[index] = NodeHandle{};
  lastChildren[index] = NodeHandle{};
  prevSiblings[index] = NodeHandle{};
  nextSiblings[index] = NodeHandle{};

  childCounts[index] = 0;
  rects[index] = Rect::zero();
  scrollOffsetsY[index] = 0.0f;
  layoutStyles[index] = LayoutStyle{};
  styleHandles[index] = StyleHandle{};
  layoutResults[index] = LayoutResult{};

  textStateIndices[index] = InvalidIndex;
  sliderStateIndices[index] = InvalidIndex;
  textInputStateIndices[index] = InvalidIndex;
  separatorStateIndices[index] = InvalidIndex;
  splitContainerStateIndices[index] = InvalidIndex;
  numericInputStateIndices[index] = InvalidIndex;
  comboBoxStateIndices[index] = InvalidIndex;
  collapsibleSectionStateIndices[index] = InvalidIndex;
  progressBarStateIndices[index] = InvalidIndex;
  treeViewStateIndices[index] = InvalidIndex;
  iconStateIndices[index] = InvalidIndex;
  zLayers[index] = 0;

  switch (type) {
    case NodeType::Slider:
      addSparseState(index, sliderStates, sliderOwners, sliderStateIndices);
      break;

    case NodeType::TextInput:
      addSparseState(index, textInputStates, textInputOwners, textInputStateIndices);
      break;

    case NodeType::NumericInput:
      addSparseState(index, textInputStates, textInputOwners, textInputStateIndices);
      addSparseState(index, numericInputStates, numericInputOwners, numericInputStateIndices);
      break;

    case NodeType::Separator:
      addSparseState(index, separatorStates, separatorOwners, separatorStateIndices);
      break;

    case NodeType::SplitContainer:
      addSparseState(index, splitContainerStates, splitContainerOwners,
                     splitContainerStateIndices);
      break;

    case NodeType::ComboBox:
      addSparseState(index, comboBoxStates, comboBoxOwners, comboBoxStateIndices);
      break;

    case NodeType::CollapsibleSection:
      addSparseState(index, collapsibleSectionStates, collapsibleSectionOwners,
                    collapsibleSectionStateIndices);
      break;

    case NodeType::ProgressBar:
      addSparseState(index, progressBarStates, progressBarOwners, progressBarStateIndices);
      break;

    case NodeType::TreeView:
      addSparseState(index, treeViewStates, treeViewOwners, treeViewStateIndices);
      break;
      
    case NodeType::Icon:
      addSparseState(index, iconStates, iconOwners, iconStateIndices);
      break;

    default:
      break;
  }
  ++aliveCount;
}

void Tree::freeSlot(u32 index) {
  removeSparseState(index, InvalidIndex, textStates, textOwners, textStateIndices);
  removeSparseState(index, InvalidIndex, sliderStates, sliderOwners, sliderStateIndices);
  removeSparseState(index, InvalidIndex, textInputStates, textInputOwners, textInputStateIndices);
  removeSparseState(index, InvalidIndex, numericInputStates, numericInputOwners, numericInputStateIndices);
  removeSparseState(index, InvalidIndex, separatorStates, separatorOwners, separatorStateIndices);
  removeSparseState(index, InvalidIndex, splitContainerStates,
                    splitContainerOwners, splitContainerStateIndices);
  removeSparseState(index, InvalidIndex, comboBoxStates, comboBoxOwners, comboBoxStateIndices);
  removeSparseState(index, InvalidIndex, collapsibleSectionStates, collapsibleSectionOwners,
                    collapsibleSectionStateIndices);
  removeSparseState(index, InvalidIndex, progressBarStates, progressBarOwners, progressBarStateIndices);
  removeSparseState(index, InvalidIndex, treeViewStates, treeViewOwners, treeViewStateIndices);
  removeSparseState(index, InvalidIndex,iconStates,iconOwners, iconStateIndices);
  
  releaseStyle(styleHandles[index]);
  styleHandles[index] = StyleHandle{};

  alive[index] = 0;
  zLayers[index] = 0;
  scrollOffsetsY[index] = 0.0f;

  ++generations[index];

  if (generations[index] == 0) {
    generations[index] = 1;
  }

  freeList.push_back(index);

  if (aliveCount > 0) {
    --aliveCount;
  }
}

TextState& Tree::ensureTextState(u32 nodeIndex) {
  const u32 existingIndex = textStateIndices[nodeIndex];

  if (existingIndex != InvalidIndex) {
    return textStates[existingIndex];
  }

  return addSparseState(nodeIndex, textStates, textOwners, textStateIndices);
}

void Tree::linkChild(NodeHandle child, NodeHandle parent) {
  if (!isValid(child) || !isValid(parent)) {
    return;
  }

  const u32 childIndex = child.index;
  const u32 parentIndex = parent.index;

  const NodeHandle last = lastChildren[parentIndex];

  parents[childIndex] = parent;
  prevSiblings[childIndex] = last;
  nextSiblings[childIndex] = NodeHandle{};

  if (isValid(last)) {
    nextSiblings[last.index] = child;
  } else {
    firstChildren[parentIndex] = child;
  }

  lastChildren[parentIndex] = child;

  ++childCounts[parentIndex];
  markLayoutDirty(parent);
  markPaintDirty(parent);
}

void Tree::unlink(NodeHandle handle) {
  if (!isValid(handle)) {
    return;
  }

  const NodeHandle parent = parents[handle.index];

  if (!isValid(parent)) {
    return;
  }

  const u32 index = handle.index;
  const u32 parentIndex = parent.index;

  const NodeHandle prev = prevSiblings[index];
  const NodeHandle next = nextSiblings[index];

  if (isValid(prev)) {
    nextSiblings[prev.index] = next;
  } else {
    firstChildren[parentIndex] = next;
  }

  if (isValid(next)) {
    prevSiblings[next.index] = prev;
  } else {
    lastChildren[parentIndex] = prev;
  }

  parents[index] = NodeHandle{};
  prevSiblings[index] = NodeHandle{};
  nextSiblings[index] = NodeHandle{};

  if (childCounts[parentIndex] > 0) {
    --childCounts[parentIndex];
  }

  markLayoutDirty(parent);
  markPaintDirty(parent);
}

f32 Tree::maxScrollOffsetY(NodeHandle handle) const noexcept {
  if (!isValid(handle)) {
    return 0.0f;
  }

  const LayoutStyle &style = layoutStyles[handle.index];

  if (!style.scrollY) {
    return 0.0f;
  }

  f32 viewportHeight = rects[handle.index].h - style.padding.vertical();

  if (viewportHeight < 0.0f) {
    viewportHeight = 0.0f;
  }

  const f32 maxOffset =
      layoutResults[handle.index].contentSize.y - viewportHeight;

  return maxOffset > 0.0f ? maxOffset : 0.0f;
}

bool Tree::setScrollOffsetY(NodeHandle handle, f32 offset) noexcept {
  if (!isValid(handle)) {
    return false;
  }

  const f32 maxOffset = maxScrollOffsetY(handle);

  if (offset < 0.0f) {
    offset = 0.0f;
  }

  if (offset > maxOffset) {
    offset = maxOffset;
  }

  f32 &current = scrollOffsetsY[handle.index];

  if (current == offset) {
    return false;
  }

  current = offset;
  markPaintDirty(handle);

  return true;
}

i32 Tree::zLayer(NodeHandle handle) const noexcept {
  if (!isValid(handle)) {
    return 0;
  }

  return zLayers[handle.index];
}

void Tree::setZLayer(NodeHandle handle, i32 layer) {
  if (!isValid(handle)) {
    return;
  }

  i32 &current = zLayers[handle.index];

  if (current == layer) {
    return;
  }

  current = layer;

  markPaintDirty(handle);
}

} // namespace octogui