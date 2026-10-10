#pragma once

#include <memory>
#include <string_view>

#include "OctoGui/DrawList.h"
#include "OctoGui/Event.h"
#include "OctoGui/Font.h"
#include "OctoGui/Input.h"
#include "OctoGui/LayoutProxy.h"
#include "OctoGui/Style.h"
#include "OctoGui/StyleProxy.h"
#include "OctoGui/TextureBackend.h"
#include "OctoGui/Tree.h"
#include "OctoGui/Types.h"

namespace octogui {
class Context {
public:
  Context();
  ~Context();

  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  Context(Context &&) noexcept = delete;
  Context &operator=(Context &&) noexcept = delete;

  [[nodiscard]]
  InputState &input() noexcept;

  [[nodiscard]]
  const InputState &input() const noexcept;

  [[nodiscard]]
  DrawList &drawList() noexcept;

  [[nodiscard]]
  const DrawList &drawList() const noexcept;

  [[nodiscard]]
  Tree &tree() noexcept;

  [[nodiscard]]
  const Tree &tree() const noexcept;

  [[nodiscard]]
  bool isValid(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isVisible(NodeHandle handle) const noexcept;

  void setVisible(NodeHandle handle, bool visible);

  [[nodiscard]]
  bool isEnabled(NodeHandle handle) const noexcept;

  void setEnabled(NodeHandle handle, bool enabled);

  [[nodiscard]]
  const Theme &theme() const noexcept;

  void setTheme(const Theme &theme);

  [[nodiscard]]
  u64 frameIndex() const noexcept;

  [[nodiscard]]
  i32 zLayer(NodeHandle handle) const noexcept;

  void setZLayer(NodeHandle handle, i32 layer);

  [[nodiscard]]
  f32 scrollOffsetY(NodeHandle handle) const noexcept;

  [[nodiscard]]
  f32 maxScrollOffsetY(NodeHandle handle) const noexcept;

  void setScrollOffsetY(NodeHandle handle, f32 offset);

  void beginFrame(f32 deltaTime);

  void layout();

  void paint();

  void endFrame();

  [[nodiscard]]
  bool pollEvent(UIEvent &event) noexcept;

  [[nodiscard]]
  bool hasEvents() const noexcept;

  [[nodiscard]]
  usize pendingEventCount() const noexcept;

  void clearEvents() noexcept;

  [[nodiscard]]
  NodeHandle createCustomNode(NodeHandle parent = {});

  [[nodiscard]]
  NodeHandle createContainer(NodeHandle parent = {});

  [[nodiscard]]
  NodeHandle createPanel(NodeHandle parent = {});

  [[nodiscard]]
  NodeHandle createButton(NodeHandle parent = {}, std::string_view text = {});

  [[nodiscard]]
  NodeHandle createLabel(NodeHandle parent = {}, std::string_view text = {});

  [[nodiscard]]
  NodeHandle createIcon(NodeHandle parent = {}, std::string_view name = {});

  void setIcon(NodeHandle handle, std::string_view name);
  void setIconColor(NodeHandle handle, Color color);
  void setIconSize(NodeHandle handle, f32 size);

  [[nodiscard]]
  NodeHandle createCheckbox(NodeHandle parent = {}, std::string_view text = {},
                            bool checked = false);

  [[nodiscard]]
  NodeHandle createSlider(NodeHandle parent = {}, f32 value = 0.0f,
                          f32 minValue = 0.0f, f32 maxValue = 1.0f);

  [[nodiscard]]
  NodeHandle createProgressBar(NodeHandle parent = {}, f64 value = 0.0,
                               f64 minValue = 0.0, f64 maxValue = 1.0);

  [[nodiscard]]
  f64 progressValue(NodeHandle handle) const noexcept;

  void setProgressValue(NodeHandle handle, f64 value);
  void setProgressRange(NodeHandle handle, f64 minValue, f64 maxValue);
  void setProgressShowPercentage(NodeHandle handle, bool showPercentage);

  [[nodiscard]]
  NodeHandle createRadioButton(NodeHandle parent = {},
                               std::string_view text = {},
                               bool checked = false);

  void selectRadioButton(NodeHandle handle);

  [[nodiscard]]
  NodeHandle createTextInput(NodeHandle parent = {}, std::string_view text = {}, std::string_view placeholder = {});

  [[nodiscard]]
  NodeHandle createSeparator(NodeHandle parent = {}, SeparatorOrientation orientation = SeparatorOrientation::Horizontal);

  void setSeparatorOrientation(NodeHandle handle, SeparatorOrientation orientation);

  [[nodiscard]]
  NodeHandle createSplitContainer(
      NodeHandle parent = {},
      SplitOrientation orientation = SplitOrientation::Horizontal,
      f32 ratio = 0.5f);

  [[nodiscard]]
  NodeHandle splitFirstPane(NodeHandle handle) const noexcept;

  [[nodiscard]]
  NodeHandle splitSecondPane(NodeHandle handle) const noexcept;

  [[nodiscard]]
  f32 splitRatio(NodeHandle handle) const noexcept;

  void setSplitRatio(NodeHandle handle, f32 ratio);

  void setSplitOrientation(NodeHandle handle, SplitOrientation orientation);

  void setSplitMinimumSizes(NodeHandle handle, f32 first, f32 second);

  [[nodiscard]]
  NodeHandle createNumericInput(NodeHandle parent = {}, f64 value = 0.0, NumericInputType type = NumericInputType::Float);

  [[nodiscard]]
  f64 numericValue(NodeHandle handle) const noexcept;

  void setNumericValue(NodeHandle handle, f64 value);
  void setNumericRange(NodeHandle handle, f64 minValue, f64 maxValue);
  void setNumericStep(NodeHandle handle, f64 step);
  void setNumericType(NodeHandle handle, NumericInputType type);

  [[nodiscard]]
  NodeHandle createComboBox(NodeHandle parent, const std::vector<std::string>& items, i32 selectedIndex = -1);

  [[nodiscard]]
  i32 comboBoxSelectedIndex(NodeHandle handle) const noexcept;

  [[nodiscard]]
  std::string_view comboBoxSelectedText(NodeHandle handle) const noexcept;

  void setComboBoxSelectedIndex(NodeHandle handle, i32 index);

  void setComboBoxItems(NodeHandle handle, const std::vector<std::string>& items); 
  
  [[nodiscard]]
  NodeHandle createCollapsibleSection(NodeHandle parent, std::string_view title, bool collapsed = false);

  [[nodiscard]]
  NodeHandle collapsibleSectionContent(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isCollapsibleSectionCollapsed(NodeHandle handle) const noexcept;

  void setCollapsibleSectionCollapsed(NodeHandle handle, bool collapsed);

  void toggleCollapsibleSection(NodeHandle handle);

  [[nodiscard]]
  NodeHandle createTreeView(NodeHandle parent = {});

  [[nodiscard]]
  TreeViewItemId addTreeViewItem(NodeHandle treeView, TreeViewItemId parent,
                                 std::string_view text, u64 userValue = 0);

  bool removeTreeViewItem(NodeHandle treeView, TreeViewItemId item);

  void clearTreeView(NodeHandle treeView);

  [[nodiscard]]
  bool treeViewItemExists(NodeHandle treeView,
                          TreeViewItemId item) const noexcept;

  [[nodiscard]]
  u32 treeViewItemCount(NodeHandle treeView) const noexcept;

  [[nodiscard]]
  TreeViewItemId treeViewSelectedItem(NodeHandle treeView) const noexcept;

  void setTreeViewSelectedItem(NodeHandle treeView, TreeViewItemId item);

  [[nodiscard]]
  std::string_view treeViewItemText(NodeHandle treeView,
                                    TreeViewItemId item) const noexcept;

  void setTreeViewItemText(NodeHandle treeView, TreeViewItemId item,
                           std::string_view text);

  [[nodiscard]]
  u64 treeViewItemUserValue(NodeHandle treeView,
                            TreeViewItemId item) const noexcept;

  void setTreeViewItemUserValue(NodeHandle treeView, TreeViewItemId item,
                                u64 value);

  [[nodiscard]]
  bool isTreeViewItemExpanded(NodeHandle treeView,
                              TreeViewItemId item) const noexcept;

  void setTreeViewItemExpanded(NodeHandle treeView, TreeViewItemId item,
                               bool expanded);

  [[nodiscard]]
  NodeHandle root() const noexcept;

  void setRoot(NodeHandle handle) noexcept;

  NodeHandle ensureRoot();

  [[nodiscard]]
  NodeHandle hovered() const noexcept;

  [[nodiscard]]
  NodeHandle active() const noexcept;

  [[nodiscard]]
  NodeHandle focused() const noexcept;

  [[nodiscard]]
  NodeHandle pressed() const noexcept;

  [[nodiscard]]
  NodeHandle released() const noexcept;

  [[nodiscard]]
  NodeHandle clicked() const noexcept;

  [[nodiscard]]
  bool isHovered(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isActive(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isFocused(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isPressed(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isReleased(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isClicked(NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isChecked(NodeHandle handle) const noexcept;

  void setChecked(NodeHandle handle, bool checked);

  void toggleChecked(NodeHandle handle);

  [[nodiscard]]
  f32 sliderValue(NodeHandle handle) const noexcept;

  void setSliderValue(NodeHandle handle, f32 value);

  void setSliderRange(NodeHandle handle, f32 minValue, f32 maxValue);

  void setSliderStep(NodeHandle handle, f32 step);

  void setFocused(NodeHandle handle);

  void clearFocus();

  void setTextureBackend(TextureBackend *backend) noexcept;

  [[nodiscard]]
  bool loadDefaultFont(std::string_view path, u32 pixelSize = 18);

  [[nodiscard]]
  bool loadDefaultIconAtlas(std::string_view path);
  [[nodiscard]]
  bool loadIcon(std::string_view name,
                std::string_view path,
                IconColorMode colorMode = IconColorMode::Tintable);

  [[nodiscard]]
  Font &font() noexcept;

  [[nodiscard]]
  const Font &font() const noexcept;

  void setText(NodeHandle handle, std::string_view value);

  [[nodiscard]]
  std::string_view text(NodeHandle handle) const noexcept;

  void setTextAlign(NodeHandle handle, TextAlign horizontal,
                    TextAlign vertical = TextAlign::Center);

  void setTextInputPlaceholder(NodeHandle handle, std::string_view placeholder);

  [[nodiscard]]
  std::string_view textInputPlaceholder(NodeHandle handle) const noexcept;

  void setTextInputReadOnly(NodeHandle handle, bool readOnly);

  [[nodiscard]]
  bool isTextInputReadOnly(NodeHandle handle) const noexcept;

  void setTextInputCharacterLimit(NodeHandle handle, u32 maxCharacters);

  [[nodiscard]]
  u32 textInputCharacterLimit(NodeHandle handle) const noexcept;

  void destroyNode(NodeHandle handle);

  [[nodiscard]]
  StyleProxy style(NodeHandle handle) noexcept;

  [[nodiscard]]
  StyleProxy defaultStyle(NodeType type) noexcept;

  [[nodiscard]]
  LayoutProxy layoutStyle(NodeHandle handle) noexcept;

  void resetDefaultStyles() noexcept;

private:
  struct Impl;

  std::unique_ptr<Impl> impl;
};

} // namespace octogui