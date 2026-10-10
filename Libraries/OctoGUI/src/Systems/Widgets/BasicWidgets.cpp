#include "Systems/StyleSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/WidgetMetrics.h"
namespace octogui {

// Panel

NodeHandle WidgetSystem::createPanel(Tree &tree, StyleSystem &styles,
                                     NodeHandle parent) {
  NodeHandle handle = createNode(tree, styles, NodeType::Container, parent);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->mode = LayoutMode::Vertical;
    layout->gap = 8.0f;
    layout->padding = Padding{8.0f};
    layout->horizontalAlignment = Alignment::Stretch;
  }

  return handle;
}

// Button

NodeHandle WidgetSystem::createButton(Tree &tree, StyleSystem &styles,
                                      NodeHandle parent,
                                      std::string_view text) {
  NodeHandle handle = createNode(tree, styles, NodeType::Button, parent);

  tree.setTextAlign(handle, TextAlign::Center, TextAlign::Center);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize.y = 40.0f;
    layout->minSize.y = 24.0f;
  }

  if (!text.empty()) {
    tree.setText(handle, text);
  }

  return handle;
}

// Label

NodeHandle WidgetSystem::createLabel(Tree &tree, StyleSystem &styles,
                                     NodeHandle parent, std::string_view text) {
  NodeHandle handle = createNode(tree, styles, NodeType::Label, parent);

  tree.setTextAlign(handle, TextAlign::Start, TextAlign::Center);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize.y = 20.0f;
    layout->minSize.y = 12.0f;
  }

  if (!text.empty()) {
    tree.setText(handle, text);
  }

  return handle;
}

// Icon

NodeHandle WidgetSystem::createIcon(
    Tree& tree,
    StyleSystem& styles,
    NodeHandle parent,
    std::string_view name) {
  NodeHandle handle =
      createNode(
          tree,
          styles,
          NodeType::Icon,
          parent);

  IconState* state =
      tree.iconState(handle);

  if (!state) {return handle;}

  state->name.assign(
      name.data(),
      name.size());

  LayoutStyle* layout =
    tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize =
        Vec2{
            detail::IconDefaultSize,
            detail::IconDefaultSize};

    layout->horizontalAlignment =
        Alignment::Start;

    layout->verticalAlignment =
        Alignment::Center;
  }

  return handle;
}

void WidgetSystem::setIcon(
    Tree& tree,
    NodeHandle handle,
    std::string_view name) {
  if (!tree.isValid(handle) ||
      tree.type(handle) != NodeType::Icon) {
    return;
  }

  IconState* state =
      tree.iconState(handle);

  if (!state || state->name == name) {return;}

  state->name.assign(
      name.data(),
      name.size());

  tree.markPaintDirty(handle);
}

void WidgetSystem::setIconColor(
    Tree& tree,
    NodeHandle handle,
    Color color) {
  if (!tree.isValid(handle) ||
      tree.type(handle) != NodeType::Icon) {
    return;
  }

  IconState* state =
      tree.iconState(handle);

  if (!state) {return;}

  state->color = color;

  tree.markPaintDirty(handle);
}

void WidgetSystem::setIconSize(
    Tree& tree,
    NodeHandle handle,
    f32 size) {
  if (!tree.isValid(handle) ||
      tree.type(handle) != NodeType::Icon ||
      size <= 0.0f) {
    return;
  }

  LayoutStyle* layout =
      tree.layoutStyle(handle);

  if (!layout) {
    return;
  }

  const Vec2 newSize{
      size,
      size};

  if (layout->preferredSize.x == size &&
      layout->preferredSize.y == size) {
    return;
  }

  layout->preferredSize =
      newSize;

  tree.markLayoutDirty(handle);
  tree.markPaintDirty(handle);
}

// Separator

NodeHandle WidgetSystem::createSeparator(Tree& tree, StyleSystem& styles,
                                         NodeHandle parent,
                                         SeparatorOrientation orientation) {
  NodeHandle handle = createNode(
      tree,
      styles,
      NodeType::Separator,
      parent
  );

  SeparatorState* state = tree.separatorState(handle);
  if (!state) {return handle;}

  state->orientation = orientation;

  LayoutStyle* layout = tree.layoutStyle(handle);
  if (!layout) {return handle;}

  if (orientation == SeparatorOrientation::Horizontal) {
    layout->minSize = Vec2{0.0f, 1.0f};
    layout->preferredSize = Vec2{0.0f, 1.0f};
    layout->horizontalAlignment = Alignment::Stretch;
    layout->verticalAlignment = Alignment::Start;
  } else {
    layout->minSize = Vec2{1.0f, 0.0f};
    layout->preferredSize = Vec2{1.0f, 0.0f};
    layout->horizontalAlignment = Alignment::Start;
    layout->verticalAlignment = Alignment::Stretch;
  }

  tree.markLayoutDirty(handle);
  tree.markPaintDirty(handle);

  return handle;
}

void WidgetSystem::setSeparatorOrientation(Tree& tree, NodeHandle handle,
                                           SeparatorOrientation orientation) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::Separator) {return;}

  SeparatorState* state = tree.separatorState(handle);
  LayoutStyle* layout = tree.layoutStyle(handle);

  if (!state || !layout || state->orientation == orientation) {return;}

  state->orientation = orientation;

  if (orientation == SeparatorOrientation::Horizontal) {
    layout->minSize = Vec2{0.0f, 1.0f};
    layout->preferredSize = Vec2{0.0f, 1.0f};
    layout->horizontalAlignment = Alignment::Stretch;
    layout->verticalAlignment = Alignment::Start;
  } else {
    layout->minSize = Vec2{1.0f, 0.0f};
    layout->preferredSize = Vec2{1.0f, 0.0f};
    layout->horizontalAlignment = Alignment::Start;
    layout->verticalAlignment = Alignment::Stretch;
  }

  tree.markLayoutDirty(handle);
  tree.markPaintDirty(handle);
}

NodeHandle WidgetSystem::createProgressBar(
    Tree& tree,
    StyleSystem& styles,
    NodeHandle parent,
    f64 value,
    f64 minValue,
    f64 maxValue) {
  NodeHandle handle =
      createNode(tree, styles, NodeType::ProgressBar, parent);

  LayoutStyle* layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize.y = detail::ProgressBarHeight;
    layout->minSize.y = detail::ProgressBarMinHeight;
  }

  ProgressBarState* state = tree.progressBarState(handle);

  if (!state) {
    return handle;
  }

  if (maxValue < minValue) {
    const f64 temporary = minValue;
    minValue = maxValue;
    maxValue = temporary;
  }

  state->minValue = minValue;
  state->maxValue = maxValue;

  if (value < minValue) {
    value = minValue;
  }

  if (value > maxValue) {
    value = maxValue;
  }

  state->value = value;

  return handle;
}

f64 WidgetSystem::progressValue(
    const Tree& tree,
    NodeHandle handle) const noexcept {
  const ProgressBarState* state = tree.progressBarState(handle);

  return state ? state->value : 0.0;
}

void WidgetSystem::setProgressValue(
    Tree& tree,
    NodeHandle handle,
    f64 value) {
  ProgressBarState* state = tree.progressBarState(handle);

  if (!state) {
    return;
  }

  if (value < state->minValue) {
    value = state->minValue;
  }

  if (value > state->maxValue) {
    value = state->maxValue;
  }

  if (state->value == value) {
    return;
  }

  state->value = value;
  tree.markPaintDirty(handle);
}

void WidgetSystem::setProgressRange(
    Tree& tree,
    NodeHandle handle,
    f64 minValue,
    f64 maxValue) {
  ProgressBarState* state = tree.progressBarState(handle);

  if (!state) {
    return;
  }

  if (maxValue < minValue) {
    const f64 temporary = minValue;
    minValue = maxValue;
    maxValue = temporary;
  }

  const bool rangeChanged =
      state->minValue != minValue || state->maxValue != maxValue;

  state->minValue = minValue;
  state->maxValue = maxValue;

  f64 value = state->value;

  if (value < minValue) {
    value = minValue;
  }

  if (value > maxValue) {
    value = maxValue;
  }

  const bool valueChanged = state->value != value;

  state->value = value;

  if (rangeChanged || valueChanged) {
    tree.markPaintDirty(handle);
  }
}

void WidgetSystem::setProgressShowPercentage(
    Tree& tree,
    NodeHandle handle,
    bool showPercentage) {
  ProgressBarState* state = tree.progressBarState(handle);

  if (!state || state->showPercentage == showPercentage) {
    return;
  }

  state->showPercentage = showPercentage;
  tree.markPaintDirty(handle);
}

NodeHandle WidgetSystem::createCollapsibleSection(
    Tree& tree,
    StyleSystem& styles,
    NodeHandle parent,
    std::string_view title,
    bool collapsed) {
  NodeHandle handle =
      createNode(
          tree,
          styles,
          NodeType::CollapsibleSection,
          parent);

  tree.setInteractive(handle, true);
  tree.setFocusable(handle, true);

  tree.setTextAlign(
      handle,
      TextAlign::Start,
      TextAlign::Center);

  LayoutStyle* headerLayout =
      tree.layoutStyle(handle);

  if (headerLayout) {
    headerLayout->preferredSize.y =
        detail::CollapsibleSectionHeight;

    headerLayout->minSize.y =
        detail::CollapsibleSectionMinHeight;

    headerLayout->padding =
        Padding{
            detail::CollapsibleSectionTextInset,
            6.0f,
            10.0f,
            6.0f};
  }

  if (!title.empty()) {
    tree.setText(handle, title);
  }

  NodeHandle content =
      createNode(
          tree,
          styles,
          NodeType::Container,
          parent);

  LayoutStyle* contentLayout =
      tree.layoutStyle(content);

  if (contentLayout) {
    contentLayout->mode =
        LayoutMode::Vertical;

    contentLayout->gap =
        8.0f;

    contentLayout->padding =
        Padding{12.0f, 6.0f, 0.0f, 8.0f};

    contentLayout->horizontalAlignment =
        Alignment::Stretch;

    contentLayout->fitContentY =
      true;
  }

  CollapsibleSectionState* state =
      tree.collapsibleSectionState(handle);

  if (!state) {
    return handle;
  }

  state->content = content;

  tree.setVisible(
      content,
      !collapsed);

  return handle;
}

NodeHandle WidgetSystem::collapsibleSectionContent(
    const Tree& tree,
    NodeHandle handle) const noexcept {
  const CollapsibleSectionState* state =
      tree.collapsibleSectionState(handle);

  if (!state ||
      !tree.isValid(state->content)) {
    return {};
  }

  return state->content;
}

bool WidgetSystem::isCollapsibleSectionCollapsed(
    const Tree& tree,
    NodeHandle handle) const noexcept {
  const CollapsibleSectionState* state =
      tree.collapsibleSectionState(handle);

  if (!state ||
      !tree.isValid(state->content)) {
    return true;
  }

  return !tree.isVisible(
      state->content);
}

void WidgetSystem::setCollapsibleSectionCollapsed(
    Tree& tree,
    NodeHandle handle,
    bool collapsed) {
  CollapsibleSectionState* state =
      tree.collapsibleSectionState(handle);

  if (!state ||
      !tree.isValid(state->content)) {
    return;
  }

  const bool currentlyCollapsed =
      !tree.isVisible(state->content);

  if (currentlyCollapsed == collapsed) {
    return;
  }

  tree.setVisible(
      state->content,
      !collapsed);

  tree.markPaintDirty(handle);
}

void WidgetSystem::toggleCollapsibleSection(
    Tree& tree,
    NodeHandle handle) {
  setCollapsibleSectionCollapsed(
      tree,
      handle,
      !isCollapsibleSectionCollapsed(
          tree,
          handle));
}

NodeHandle WidgetSystem::createSplitContainer(
    Tree& tree, StyleSystem& styles, NodeHandle parent, SplitOrientation orientation, f32 ratio) {
    NodeHandle handle = createNode(tree, styles, NodeType::SplitContainer, parent);

    LayoutStyle* layout = tree.layoutStyle(handle);

    if (layout) {
        layout->mode = LayoutMode::Manual;
        layout->padding = Padding{0.0f};
        layout->horizontalAlignment = Alignment::Stretch;
        layout->verticalAlignment = Alignment::Stretch;
    }

    SplitContainerState* state = tree.splitContainerState(handle);

    if (!state) {
        return handle;
    }

    state->orientation = orientation;
    state->ratio = ratio < 0.0f ? 0.0f : ratio > 1.0f ? 1.0f : ratio;
    state->minFirst = detail::SplitDefaultMinPaneSize;
    state->minSecond = detail::SplitDefaultMinPaneSize;

    state->firstPane = createPanel(tree, styles, handle);
    state->secondPane = createPanel(tree, styles, handle);

    return handle;
}

NodeHandle WidgetSystem::splitFirstPane(const Tree& tree, NodeHandle handle) const noexcept {
    const SplitContainerState* state = tree.splitContainerState(handle);

    return state && tree.isValid(state->firstPane) ? state->firstPane : NodeHandle{};
}

NodeHandle WidgetSystem::splitSecondPane(const Tree& tree, NodeHandle handle) const noexcept {
    const SplitContainerState* state = tree.splitContainerState(handle);

    return state && tree.isValid(state->secondPane) ? state->secondPane : NodeHandle{};
}

f32 WidgetSystem::splitRatio(const Tree& tree, NodeHandle handle) const noexcept {
    const SplitContainerState* state = tree.splitContainerState(handle);

    return state ? state->ratio : 0.5f;
}

void WidgetSystem::setSplitRatio(Tree& tree, NodeHandle handle, f32 ratio) {
    SplitContainerState* state = tree.splitContainerState(handle);

    if (!state) {
        return;
    }

    if (ratio < 0.0f) {
        ratio = 0.0f;
    }

    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    if (state->ratio == ratio) {
        return;
    }

    state->ratio = ratio;

    tree.markLayoutDirty(handle);
    tree.markPaintDirty(handle);
}

void WidgetSystem::setSplitOrientation(Tree& tree, NodeHandle handle, SplitOrientation orientation) {
    SplitContainerState* state = tree.splitContainerState(handle);

    if (!state || state->orientation == orientation) {
        return;
    }

    state->orientation = orientation;

    tree.markLayoutDirty(handle);
    tree.markPaintDirty(handle);
}

void WidgetSystem::setSplitMinimumSizes(
    Tree& tree, NodeHandle handle, f32 first, f32 second) {
    SplitContainerState* state = tree.splitContainerState(handle);

    if (!state) {
        return;
    }

    if (first < 0.0f) {
        first = 0.0f;
    }

    if (second < 0.0f) {
        second = 0.0f;
    }

    if (state->minFirst == first && state->minSecond == second) {
        return;
    }

    state->minFirst = first;
    state->minSecond = second;

    tree.markLayoutDirty(handle);
    tree.markPaintDirty(handle);
}

} // namespace octogui
