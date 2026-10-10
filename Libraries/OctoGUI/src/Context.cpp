#include <vector>

#include "OctoGui/Context.h"
#include "Internal/ContextImpl.h"
namespace octogui {
namespace {

void markIconPaintDirty(
    Tree &tree,
    NodeHandle root,
    std::string_view name) {
  if (!tree.isValid(root)) {
    return;
  }

  std::vector<NodeHandle> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    const NodeHandle node = stack.back();
    stack.pop_back();

    if (tree.type(node) == NodeType::Icon) {
      const IconState *state = tree.iconState(node);

      if (state && state->name == name) {
        tree.markPaintDirty(node);
      }
    }

    NodeHandle child = tree.firstChild(node);

    while (tree.isValid(child)) {
      stack.push_back(child);
      child = tree.nextSibling(child);
    }
  }
}

void markAllIconsPaintDirty(Tree &tree, NodeHandle root) {
  if (!tree.isValid(root)) {
    return;
  }

  std::vector<NodeHandle> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    const NodeHandle node = stack.back();
    stack.pop_back();

    if (tree.type(node) == NodeType::Icon) {
      tree.markPaintDirty(node);
    }

    NodeHandle child = tree.firstChild(node);

    while (tree.isValid(child)) {
      stack.push_back(child);
      child = tree.nextSibling(child);
    }
  }
}

} // namespace

Context::Context() : impl(std::make_unique<Impl>()) {
  impl->styles.resetDefaults();
}

Context::~Context() = default;

InputState &Context::input() noexcept { return impl->input; }

const InputState &Context::input() const noexcept { return impl->input; }

DrawList &Context::drawList() noexcept { return impl->paint.drawList(); }

const DrawList &Context::drawList() const noexcept {
  return impl->paint.drawList();
}

Tree &Context::tree() noexcept { return impl->tree; }

const Tree &Context::tree() const noexcept { return impl->tree; }

bool Context::isValid(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle);
}

bool Context::isVisible(NodeHandle handle) const noexcept {
  return impl->tree.isVisible(handle);
}

void Context::setVisible(NodeHandle handle, bool visible) {
  impl->tree.setVisible(handle, visible);
}

bool Context::isEnabled(NodeHandle handle) const noexcept {
  return impl->tree.isEnabled(handle);
}

void Context::setEnabled(NodeHandle handle, bool enabled) {
  impl->tree.setEnabled(handle, enabled);
}

const Theme &Context::theme() const noexcept { return impl->styles.theme(); }

void Context::setTheme(const Theme &theme) {
  impl->styles.setTheme(impl->tree, impl->root, theme);
}

u64 Context::frameIndex() const noexcept { return impl->frameCounter; }

i32 Context::zLayer(NodeHandle handle) const noexcept {
  return impl->tree.zLayer(handle);
}

void Context::setZLayer(NodeHandle handle, i32 layer) {
  impl->tree.setZLayer(handle, layer);
}

f32 Context::scrollOffsetY(NodeHandle handle) const noexcept {
  return impl->tree.scrollOffsetY(handle);
}

f32 Context::maxScrollOffsetY(NodeHandle handle) const noexcept {
  return impl->tree.maxScrollOffsetY(handle);
}

void Context::setScrollOffsetY(NodeHandle handle, f32 offset) {
  impl->tree.setScrollOffsetY(handle, offset);
}

void Context::beginFrame(f32 deltaTime) {
  impl->input.deltaTime = deltaTime < 0.0f ? 0.0f : deltaTime;

  impl->paint.beginFrame();

  ++impl->frameCounter;
}

void Context::layout() {
  ensureRoot();

  impl->layout.update(impl->tree, impl->root, impl->input, impl->text);

  Font *font = impl->text.font().isLoaded() ? &impl->text.font() : nullptr;

  impl->interaction.update(impl->tree, impl->root, impl->input, impl->widgets,
                           impl->events, font);

  if (impl->tree.isLayoutDirty(impl->root) ||
      impl->tree.hasDirtyLayoutChildren(impl->root)) {
    impl->layout.update(impl->tree, impl->root, impl->input, impl->text);
  }
}

void Context::paint() {
  if (!impl->tree.isValid(impl->root)) {
    return;
  }

  Font *font = impl->text.font().isLoaded() ? &impl->text.font() : nullptr;

  impl->paint.paint(impl->tree, impl->root, impl->interaction.state(), font, impl->icons);
}

void Context::endFrame() { impl->input.endFrame(); }

bool Context::pollEvent(UIEvent &event) noexcept {
  return impl->events.poll(event);
}

bool Context::hasEvents() const noexcept { return !impl->events.empty(); }

usize Context::pendingEventCount() const noexcept {
  return impl->events.size();
}

void Context::clearEvents() noexcept { impl->events.clear(); }

NodeHandle Context::createCustomNode(NodeHandle parent) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createNode(impl->tree, impl->styles, NodeType::Custom,
                                 parent);
}

NodeHandle Context::createContainer(NodeHandle parent) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  NodeHandle handle = impl->widgets.createNode(
      impl->tree, impl->styles, NodeType::Container, parent);

  StyleProxy containerStyle = style(handle);
  containerStyle.all.background = Color::transparent();
  containerStyle.all.border = Color::transparent();
  containerStyle.shadow = ShadowStyle{};

  return handle;
}

NodeHandle Context::createPanel(NodeHandle parent) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createPanel(impl->tree, impl->styles, parent);
}

NodeHandle Context::createButton(NodeHandle parent, std::string_view text) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createButton(impl->tree, impl->styles, parent, text);
}

NodeHandle Context::createLabel(NodeHandle parent, std::string_view text) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createLabel(impl->tree, impl->styles, parent, text);
}

NodeHandle Context::createIcon(
    NodeHandle parent,
    std::string_view name) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createIcon(
      impl->tree,
      impl->styles,
      parent,
      name);
}

void Context::setIcon(
    NodeHandle handle,
    std::string_view name) {
  impl->widgets.setIcon(
      impl->tree,
      handle,
      name);
}

void Context::setIconColor(
    NodeHandle handle,
    Color color) {
  if (!impl->tree.isValid(handle) ||
      impl->tree.type(handle) != NodeType::Icon) {
    return;
  }

  const IconState* state = impl->tree.iconState(handle);

  if (state && impl->icons.isImageColor(state->name)) {
    return;
  }

  impl->widgets.setIconColor(
      impl->tree,
      handle,
      color);
}

void Context::setIconSize(
    NodeHandle handle,
    f32 size) {
  impl->widgets.setIconSize(
      impl->tree,
      handle,
      size);
}

NodeHandle Context::createCheckbox(NodeHandle parent, std::string_view text,
                                   bool checked) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createCheckbox(impl->tree, impl->styles, parent, text,
                                      checked);
}

NodeHandle Context::createSlider(NodeHandle parent, f32 value, f32 minValue,
                                 f32 maxValue) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createSlider(impl->tree, impl->styles, parent, value,
                                    minValue, maxValue);
}

NodeHandle Context::createProgressBar(NodeHandle parent, f64 value,
                                       f64 minValue, f64 maxValue) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createProgressBar(
      impl->tree, impl->styles, parent, value, minValue, maxValue);
}

f64 Context::progressValue(NodeHandle handle) const noexcept {
  return impl->widgets.progressValue(impl->tree, handle);
}

void Context::setProgressValue(NodeHandle handle, f64 value) {
  impl->widgets.setProgressValue(impl->tree, handle, value);
}

void Context::setProgressRange(NodeHandle handle, f64 minValue, f64 maxValue) {
  impl->widgets.setProgressRange(impl->tree, handle, minValue, maxValue);
}

void Context::setProgressShowPercentage(NodeHandle handle, bool showPercentage) {
  impl->widgets.setProgressShowPercentage(impl->tree, handle, showPercentage);
}

NodeHandle Context::createRadioButton(NodeHandle parent, std::string_view text,
                                      bool checked) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createRadioButton(impl->tree, impl->styles, parent, text,
                                         checked);
}

void Context::selectRadioButton(NodeHandle handle) {
  impl->widgets.selectRadioButton(impl->tree, impl->events, handle);
}

NodeHandle Context::createTextInput(NodeHandle parent, std::string_view text,
                                    std::string_view placeholder) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createTextInput(impl->tree, impl->styles, parent, text,
                                       placeholder);
}

NodeHandle Context::createNumericInput(NodeHandle parent, f64 value, NumericInputType type) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createNumericInput(
      impl->tree, impl->styles, parent, value, type);
}

NodeHandle Context::createSeparator(NodeHandle parent,
                                    SeparatorOrientation orientation) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createSeparator(
      impl->tree,
      impl->styles,
      parent,
      orientation
  );
}

void Context::setSeparatorOrientation(NodeHandle handle,
                                      SeparatorOrientation orientation) {
  impl->widgets.setSeparatorOrientation(
      impl->tree,
      handle,
      orientation
  );
}

NodeHandle Context::createSplitContainer(NodeHandle parent,
                                         SplitOrientation orientation,
                                         f32 ratio) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createSplitContainer(
      impl->tree,
      impl->styles,
      parent,
      orientation,
      ratio);
}

NodeHandle Context::splitFirstPane(NodeHandle handle) const noexcept {
  return impl->widgets.splitFirstPane(impl->tree, handle);
}

NodeHandle Context::splitSecondPane(NodeHandle handle) const noexcept {
  return impl->widgets.splitSecondPane(impl->tree, handle);
}

f32 Context::splitRatio(NodeHandle handle) const noexcept {
  return impl->widgets.splitRatio(impl->tree, handle);
}

void Context::setSplitRatio(NodeHandle handle, f32 ratio) {
  impl->widgets.setSplitRatio(impl->tree, handle, ratio);
}

void Context::setSplitOrientation(NodeHandle handle,
                                  SplitOrientation orientation) {
  impl->widgets.setSplitOrientation(impl->tree, handle, orientation);
}

void Context::setSplitMinimumSizes(NodeHandle handle, f32 first, f32 second) {
  impl->widgets.setSplitMinimumSizes(impl->tree, handle, first, second);
}

NodeHandle Context::createComboBox(
    NodeHandle parent,
    const std::vector<std::string>& items,
    i32 selectedIndex) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createComboBox(
      impl->tree,
      impl->styles,
      parent,
      items,
      selectedIndex);
}

i32 Context::comboBoxSelectedIndex(
    NodeHandle handle) const noexcept {
  return impl->widgets.comboBoxSelectedIndex(
      impl->tree,
      handle);
}

std::string_view Context::comboBoxSelectedText(
    NodeHandle handle) const noexcept {
  return impl->widgets.comboBoxSelectedText(
      impl->tree,
      handle);
}

void Context::setComboBoxSelectedIndex(
    NodeHandle handle,
    i32 index) {
  impl->widgets.setComboBoxSelectedIndex(
      impl->tree,
      impl->events,
      handle,
      index);
}

void Context::setComboBoxItems(
    NodeHandle handle,
    const std::vector<std::string>& items) {
  impl->widgets.setComboBoxItems(
      impl->tree,
      impl->events,
      handle,
      items);
}

NodeHandle Context::createCollapsibleSection(
    NodeHandle parent,
    std::string_view title,
    bool collapsed) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createCollapsibleSection(
      impl->tree,
      impl->styles,
      parent,
      title,
      collapsed);
}

NodeHandle Context::collapsibleSectionContent(
    NodeHandle handle) const noexcept {
  return impl->widgets.collapsibleSectionContent(
      impl->tree,
      handle);
}

bool Context::isCollapsibleSectionCollapsed(
    NodeHandle handle) const noexcept {
  return impl->widgets.isCollapsibleSectionCollapsed(
      impl->tree,
      handle);
}

void Context::setCollapsibleSectionCollapsed(
    NodeHandle handle,
    bool collapsed) {
  impl->widgets.setCollapsibleSectionCollapsed(
      impl->tree,
      handle,
      collapsed);
}

NodeHandle Context::createTreeView(NodeHandle parent) {
  if (!impl->tree.isValid(parent)) {
    parent = ensureRoot();
  }

  return impl->widgets.createTreeView(impl->tree, impl->styles, parent);
}

TreeViewItemId Context::addTreeViewItem(NodeHandle treeView,
                                        TreeViewItemId parent,
                                        std::string_view text,
                                        u64 userValue) {
  return impl->widgets.addTreeViewItem(impl->tree, treeView, parent, text,
                                       userValue);
}

bool Context::removeTreeViewItem(NodeHandle treeView, TreeViewItemId item) {
  return impl->widgets.removeTreeViewItem(impl->tree, impl->events, treeView,
                                          item);
}

void Context::clearTreeView(NodeHandle treeView) {
  impl->widgets.clearTreeView(impl->tree, impl->events, treeView);
}

bool Context::treeViewItemExists(NodeHandle treeView,
                                 TreeViewItemId item) const noexcept {
  return impl->widgets.treeViewItemExists(impl->tree, treeView, item);
}

u32 Context::treeViewItemCount(NodeHandle treeView) const noexcept {
  return impl->widgets.treeViewItemCount(impl->tree, treeView);
}

TreeViewItemId Context::treeViewSelectedItem(NodeHandle treeView) const noexcept {
  return impl->widgets.treeViewSelectedItem(impl->tree, treeView);
}

void Context::setTreeViewSelectedItem(NodeHandle treeView,
                                      TreeViewItemId item) {
  impl->widgets.setTreeViewSelectedItem(impl->tree, impl->events, treeView,
                                        item);
}

std::string_view Context::treeViewItemText(NodeHandle treeView,
                                           TreeViewItemId item) const noexcept {
  return impl->widgets.treeViewItemText(impl->tree, treeView, item);
}

void Context::setTreeViewItemText(NodeHandle treeView, TreeViewItemId item,
                                  std::string_view text) {
  impl->widgets.setTreeViewItemText(impl->tree, treeView, item, text);
}

u64 Context::treeViewItemUserValue(NodeHandle treeView,
                                   TreeViewItemId item) const noexcept {
  return impl->widgets.treeViewItemUserValue(impl->tree, treeView, item);
}

void Context::setTreeViewItemUserValue(NodeHandle treeView, TreeViewItemId item,
                                       u64 value) {
  impl->widgets.setTreeViewItemUserValue(impl->tree, treeView, item, value);
}

bool Context::isTreeViewItemExpanded(NodeHandle treeView,
                                     TreeViewItemId item) const noexcept {
  return impl->widgets.isTreeViewItemExpanded(impl->tree, treeView, item);
}

void Context::setTreeViewItemExpanded(NodeHandle treeView, TreeViewItemId item,
                                      bool expanded) {
  impl->widgets.setTreeViewItemExpanded(impl->tree, treeView, item, expanded);
}

void Context::toggleCollapsibleSection(
    NodeHandle handle) {
  impl->widgets.toggleCollapsibleSection(
      impl->tree,
      handle);
}

NodeHandle Context::root() const noexcept { return impl->root; }

void Context::setRoot(NodeHandle handle) noexcept { impl->root = handle; }

NodeHandle Context::ensureRoot() {
  if (!impl->tree.isValid(impl->root)) {
    impl->root = impl->tree.createRoot(NodeType::Root);

    impl->styles.applyDefault(impl->tree, impl->root, NodeType::Root);
  }

  return impl->root;
}

NodeHandle Context::hovered() const noexcept {
  return impl->interaction.state().hovered;
}

NodeHandle Context::active() const noexcept {
  return impl->interaction.state().active;
}

NodeHandle Context::focused() const noexcept {
  return impl->interaction.state().focused;
}

NodeHandle Context::pressed() const noexcept {
  return impl->interaction.state().pressed;
}

NodeHandle Context::released() const noexcept {
  return impl->interaction.state().released;
}

NodeHandle Context::clicked() const noexcept {
  return impl->interaction.state().clicked;
}

bool Context::isHovered(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle) &&
         handle == impl->interaction.state().hovered;
}

bool Context::isActive(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle) &&
         handle == impl->interaction.state().active;
}

bool Context::isFocused(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle) &&
         handle == impl->interaction.state().focused;
}

bool Context::isPressed(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle) &&
         handle == impl->interaction.state().pressed;
}

bool Context::isReleased(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle) &&
         handle == impl->interaction.state().released;
}

bool Context::isClicked(NodeHandle handle) const noexcept {
  return impl->tree.isValid(handle) &&
         handle == impl->interaction.state().clicked;
}

bool Context::isChecked(NodeHandle handle) const noexcept {
  return impl->widgets.isChecked(impl->tree, handle);
}

void Context::setChecked(NodeHandle handle, bool checked) {
  impl->widgets.setChecked(impl->tree, impl->events, handle, checked);
}

void Context::toggleChecked(NodeHandle handle) {
  impl->widgets.toggleChecked(impl->tree, impl->events, handle);
}

f32 Context::sliderValue(NodeHandle handle) const noexcept {
  return impl->widgets.sliderValue(impl->tree, handle);
}

void Context::setSliderValue(NodeHandle handle, f32 value) {
  impl->widgets.setSliderValue(impl->tree, impl->events, handle, value);
}

void Context::setSliderRange(NodeHandle handle, f32 minValue, f32 maxValue) {
  impl->widgets.setSliderRange(impl->tree, impl->events, handle, minValue,
                               maxValue);
}

void Context::setSliderStep(NodeHandle handle, f32 step) {
  impl->widgets.setSliderStep(impl->tree, impl->events, handle, step);
}

f64 Context::numericValue(NodeHandle handle) const noexcept {
  return impl->widgets.numericValue(impl->tree, handle);
}

void Context::setNumericValue(NodeHandle handle, f64 value) {
  impl->widgets.setNumericValue(
      impl->tree, impl->events, handle, value);
}

void Context::setNumericRange(NodeHandle handle, f64 minValue, f64 maxValue) {
  impl->widgets.setNumericRange(
      impl->tree, impl->events, handle, minValue, maxValue);
}

void Context::setNumericStep(NodeHandle handle, f64 step) {
  impl->widgets.setNumericStep(
      impl->tree, handle, step);
}

void Context::setNumericType(NodeHandle handle, NumericInputType type) {
  impl->widgets.setNumericType(
      impl->tree, impl->events, handle, type);
}

void Context::setTextInputCharacterLimit(NodeHandle handle, u32 maxCharacters) {
  impl->widgets.setTextInputCharacterLimit(
      impl->tree,
      handle,
      maxCharacters);
}

u32 Context::textInputCharacterLimit(NodeHandle handle) const noexcept {
  return impl->widgets.textInputCharacterLimit(
      impl->tree,
      handle);
}

void Context::setFocused(NodeHandle handle) {
  impl->interaction.setFocused(impl->tree, impl->widgets,impl->events, handle);
}

void Context::clearFocus() {
  impl->interaction.clearFocus(impl->tree, impl->widgets, impl->events);
}

void Context::setTextureBackend(TextureBackend *backend) noexcept {
  impl->text.setTextureBackend(backend);
  impl->icons.setTextureBackend(backend);
}

bool Context::loadDefaultFont(std::string_view path, u32 pixelSize) {
  const bool loaded = impl->text.loadDefaultFont(path, pixelSize);

  if (loaded && impl->tree.isValid(impl->root)) {
    impl->tree.markLayoutDirty(impl->root);
    impl->tree.markPaintDirty(impl->root);
  }

  return loaded;
}

bool Context::loadDefaultIconAtlas(std::string_view path) {
  const bool loaded =
      impl->icons.loadDefault(path);

  if (loaded &&
      impl->tree.isValid(impl->root)) {
    markAllIconsPaintDirty(
        impl->tree,
        impl->root);
  }

  return loaded;
}

bool Context::loadIcon(
    std::string_view name,
    std::string_view path,
    IconColorMode colorMode) {
  if (!impl->icons.loadCustom(name, path, colorMode)) {
    return false;
  }

  markIconPaintDirty(
      impl->tree,
      impl->root,
      name);

  return true;
}

Font &Context::font() noexcept { return impl->text.font(); }

const Font &Context::font() const noexcept { return impl->text.font(); }

void Context::setText(NodeHandle handle, std::string_view value) {
  impl->tree.setText(handle, value);
}

std::string_view Context::text(NodeHandle handle) const noexcept {
  return impl->tree.text(handle);
}

void Context::setTextAlign(NodeHandle handle, TextAlign horizontal,
                           TextAlign vertical) {
  impl->tree.setTextAlign(handle, horizontal, vertical);
}

void Context::setTextInputPlaceholder(NodeHandle handle,
                                      std::string_view placeholder) {
  impl->widgets.setTextInputPlaceholder(impl->tree, handle, placeholder);
}

std::string_view
Context::textInputPlaceholder(NodeHandle handle) const noexcept {
  return impl->widgets.textInputPlaceholder(impl->tree, handle);
}

void Context::setTextInputReadOnly(NodeHandle handle, bool readOnly) {
  impl->widgets.setTextInputReadOnly(impl->tree, handle, readOnly);
}

bool Context::isTextInputReadOnly(NodeHandle handle) const noexcept {
  return impl->widgets.isTextInputReadOnly(impl->tree, handle);
}

void Context::destroyNode(NodeHandle handle) {
  if (!impl->tree.isValid(handle)) {
    return;
  }

  if (handle == impl->root) {
    impl->root = NodeHandle{};
  }

  NodeHandle ownedContent{};

  if (impl->tree.type(handle) ==
      NodeType::CollapsibleSection) {
    ownedContent =
        impl->widgets.collapsibleSectionContent(
            impl->tree,
            handle);
  }

  if (impl->tree.isValid(ownedContent)) {
    impl->interaction.onNodeDestroyed(
        impl->tree,
        impl->events,
        ownedContent);

    impl->tree.destroy(
        ownedContent);
  }

  impl->interaction.onNodeDestroyed(impl->tree, impl->events, handle);

  impl->tree.destroy(handle);
}

StyleProxy Context::style(NodeHandle handle) noexcept {
  detail::StyleTarget target;
  target.tree = &impl->tree;
  target.handle = handle;

  return StyleProxy{target};
}

StyleProxy Context::defaultStyle(NodeType type) noexcept {
  detail::StyleTarget target;
  target.direct = &impl->styles.defaultVisualStyle(type);

  return StyleProxy{target};
}

LayoutProxy Context::layoutStyle(NodeHandle handle) noexcept {
  detail::LayoutTarget target;
  target.tree = &impl->tree;
  target.handle = handle;

  return LayoutProxy{target};
}

void Context::resetDefaultStyles() noexcept { impl->styles.resetDefaults(); }

} // namespace octogui