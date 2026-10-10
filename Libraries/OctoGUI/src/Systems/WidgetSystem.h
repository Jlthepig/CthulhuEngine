#pragma once

#include <string_view>

#include "OctoGui/Font.h"
#include "OctoGui/Input.h"
#include "OctoGui/Tree.h"

namespace octogui {

class StyleSystem;
class EventSystem;

class WidgetSystem {
public:
  [[nodiscard]]
  NodeHandle createNode(Tree &tree, StyleSystem &styles, NodeType type,
                        NodeHandle parent);

  [[nodiscard]]
  NodeHandle createPanel(Tree &tree, StyleSystem &styles, NodeHandle parent);

  [[nodiscard]]
  NodeHandle createButton(Tree &tree, StyleSystem &styles, NodeHandle parent,
                          std::string_view text);

  [[nodiscard]]
  NodeHandle createLabel(Tree &tree, StyleSystem &styles, NodeHandle parent,
                         std::string_view text);

  [[nodiscard]]
  NodeHandle createIcon(Tree &tree, StyleSystem &styles, NodeHandle parent,
                        std::string_view name);

  void setIcon(Tree &tree, NodeHandle handle, std::string_view name);
  void setIconColor(Tree &tree, NodeHandle handle, Color color);
  void setIconSize(Tree& tree, NodeHandle handle, f32 size);

  [[nodiscard]]
  NodeHandle createCheckbox(Tree &tree, StyleSystem &styles, NodeHandle parent,
                            std::string_view text, bool checked);

  [[nodiscard]]
  NodeHandle createSlider(Tree &tree, StyleSystem &styles, NodeHandle parent,
                          f32 value, f32 minValue, f32 maxValue);

  [[nodiscard]]
  NodeHandle createProgressBar(Tree &tree, StyleSystem &styles,
                               NodeHandle parent, f64 value = 0.0,
                               f64 minValue = 0.0, f64 maxValue = 1.0);

  [[nodiscard]]
  f64 progressValue(const Tree &tree, NodeHandle handle) const noexcept;

  void setProgressValue(Tree &tree, NodeHandle handle, f64 value);
  void setProgressRange(Tree &tree, NodeHandle handle, f64 minValue,
                        f64 maxValue);
  void setProgressShowPercentage(Tree &tree, NodeHandle handle,
                                 bool showPercentage);

  [[nodiscard]]
  NodeHandle createRadioButton(Tree &tree, StyleSystem &styles,
                               NodeHandle parent, std::string_view text,
                               bool checked);
  [[nodiscard]]
  NodeHandle createTextInput(Tree &tree, StyleSystem &styles, NodeHandle parent,
                             std::string_view text,
                             std::string_view placeholder);

  void setTextInputPlaceholder(Tree &tree, NodeHandle handle,
                               std::string_view placeholder);

  NodeHandle createSeparator(Tree& tree, StyleSystem& styles, NodeHandle parent, SeparatorOrientation orientation = SeparatorOrientation::Horizontal);

  void setSeparatorOrientation(Tree& tree, NodeHandle handle, SeparatorOrientation orientation);

  [[nodiscard]] NodeHandle createSplitContainer(Tree& tree,StyleSystem& styles, NodeHandle parent, 
    SplitOrientation orientation = SplitOrientation::Horizontal, f32 ratio = 0.5f);

  [[nodiscard]] NodeHandle splitFirstPane(const Tree& tree, NodeHandle handle) const noexcept;
  [[nodiscard]] NodeHandle splitSecondPane(const Tree& tree, NodeHandle handle) const noexcept;
  [[nodiscard]] f32 splitRatio(const Tree& tree, NodeHandle handle) const noexcept;

  void setSplitRatio(Tree& tree, NodeHandle handle, f32 ratio);
  void setSplitOrientation(Tree& tree, NodeHandle handle, SplitOrientation orientation);
  void setSplitMinimumSizes(Tree& tree, NodeHandle handle, f32 first, f32 second);

  [[nodiscard]]
  NodeHandle createNumericInput(Tree& tree, StyleSystem& styles, NodeHandle parent, f64 value, NumericInputType type);

  [[nodiscard]]
  f64 numericValue(const Tree& tree, NodeHandle handle) const noexcept;

  void setNumericValue(Tree& tree, EventSystem& events, NodeHandle handle, f64 value);
  void setNumericRange(Tree& tree, EventSystem& events, NodeHandle handle, f64 minValue, f64 maxValue);
  void setNumericStep(Tree& tree, NodeHandle handle, f64 step);
  void setNumericType(Tree& tree, EventSystem& events, NodeHandle handle, NumericInputType type);

  [[nodiscard]]
  NodeHandle createComboBox(Tree& tree, StyleSystem& styles, NodeHandle parent,
                            const std::vector<std::string>& items,
                            i32 selectedIndex = -1);

  [[nodiscard]]
  i32 comboBoxSelectedIndex(const Tree& tree, NodeHandle handle) const noexcept;

  [[nodiscard]]
  std::string_view comboBoxSelectedText(const Tree& tree,
                                        NodeHandle handle) const noexcept;

  void setComboBoxSelectedIndex(Tree& tree, EventSystem& events,
                                NodeHandle handle, i32 index);

  void setComboBoxItems(Tree& tree, EventSystem& events,
                        NodeHandle handle,
                        const std::vector<std::string>& items);

  void setComboBoxOpen(Tree& tree, NodeHandle handle, bool open);

  [[nodiscard]]
  bool isComboBoxOpen(const Tree& tree, NodeHandle handle) const noexcept;

  void setComboBoxHoveredIndex(Tree& tree, NodeHandle handle, i32 index);

  void setComboBoxScrollY(Tree& tree, NodeHandle handle, f32 scrollY, f32 maxScrollY);

  [[nodiscard]]
  bool selectComboBoxHovered(Tree& tree, EventSystem& events, NodeHandle handle);

  [[nodiscard]]
  NodeHandle createCollapsibleSection(Tree& tree, StyleSystem& styles, NodeHandle parent, std::string_view title, bool collapsed = false);

  [[nodiscard]]
  NodeHandle collapsibleSectionContent(const Tree& tree, NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isCollapsibleSectionCollapsed(const Tree& tree, NodeHandle handle) const noexcept;

  void setCollapsibleSectionCollapsed(Tree& tree, NodeHandle handle, bool collapsed);

  void toggleCollapsibleSection(Tree& tree, NodeHandle handle);

  [[nodiscard]]
  NodeHandle createTreeView(Tree& tree, StyleSystem& styles, NodeHandle parent);

  [[nodiscard]]
  TreeViewItemId addTreeViewItem(Tree& tree, NodeHandle handle, TreeViewItemId parent,
                                 std::string_view text, u64 userValue = 0);

  bool removeTreeViewItem(Tree& tree, EventSystem& events, NodeHandle handle,
                          TreeViewItemId item);

  void clearTreeView(Tree& tree, EventSystem& events, NodeHandle handle);

  [[nodiscard]]
  bool treeViewItemExists(const Tree& tree, NodeHandle handle, TreeViewItemId item) const noexcept;

  [[nodiscard]]
  u32 treeViewItemCount(const Tree& tree, NodeHandle handle) const noexcept;

  [[nodiscard]]
  TreeViewItemId treeViewSelectedItem(const Tree& tree, NodeHandle handle) const noexcept;

  void setTreeViewSelectedItem(Tree& tree, EventSystem& events,
                               NodeHandle handle, TreeViewItemId item);

  void updateTreeViewHover(Tree& tree, NodeHandle handle,
                           Vec2 mousePosition);

  void clearTreeViewHover(Tree& tree, NodeHandle handle);

  bool scrollTreeView(Tree& tree, NodeHandle handle,
                      f32 wheelDelta);

  bool updateTreeViewScrollbarDrag(Tree& tree, NodeHandle handle,
                                   Vec2 mousePosition);

  void beginTreeViewPress(Tree& tree, NodeHandle handle,
                          Vec2 mousePosition);

  void endTreeViewPress(Tree& tree, EventSystem& events, NodeHandle handle,
                        Vec2 mousePosition);

  void cancelTreeViewPress(Tree& tree, NodeHandle handle);

  void updateTreeViewPress(Tree& tree, NodeHandle handle, Vec2 mousePosition);

  [[nodiscard]]
  std::string_view treeViewItemText(const Tree& tree, NodeHandle handle, TreeViewItemId item) const noexcept;

  void setTreeViewItemText(Tree& tree, NodeHandle handle, TreeViewItemId item, std::string_view text);

  [[nodiscard]]
  u64 treeViewItemUserValue(const Tree& tree, NodeHandle handle, TreeViewItemId item) const noexcept;

  void setTreeViewItemUserValue(Tree& tree, NodeHandle handle, TreeViewItemId item, u64 value);

  [[nodiscard]]
  bool isTreeViewItemExpanded(const Tree& tree, NodeHandle handle, TreeViewItemId item) const noexcept;

  void setTreeViewItemExpanded(Tree& tree, NodeHandle handle, TreeViewItemId item, bool expanded);

  [[nodiscard]]
  std::string_view textInputPlaceholder(const Tree &tree,
                                        NodeHandle handle) const noexcept;

  void setTextInputReadOnly(Tree &tree, NodeHandle handle, bool readOnly);

  [[nodiscard]]
  bool isTextInputReadOnly(const Tree &tree, NodeHandle handle) const noexcept;

  [[nodiscard]]
  bool isChecked(const Tree &tree, NodeHandle handle) const noexcept;

  void setChecked(Tree &tree, EventSystem &events, NodeHandle handle,
                  bool checked);
  void toggleChecked(Tree &tree, EventSystem &events, NodeHandle handle);
  void selectRadioButton(Tree &tree, EventSystem &events, NodeHandle handle);

  [[nodiscard]]
  f32 sliderValue(const Tree &tree, NodeHandle handle) const noexcept;

  void setSliderValue(Tree &tree, EventSystem &events, NodeHandle handle,
                      f32 value);
  void setSliderRange(Tree &tree, EventSystem &events, NodeHandle handle,
                      f32 minValue, f32 maxValue);
  void setSliderStep(Tree &tree, EventSystem &events, NodeHandle handle,
                     f32 step);

  void handleClick(Tree &tree, EventSystem &events, NodeHandle handle);
  void updateSliderFromMouse(Tree &tree, EventSystem &events, NodeHandle handle,
                             Vec2 mousePosition);

  void handleTextInputEvent(Tree &tree, Font &font, NodeHandle handle,
                            const InputEvent &event, const ClipboardCallbacks& clipboard);
                            
  void handleNumericInputEvent(Tree& tree, Font& font, EventSystem& events, NodeHandle handle, const InputEvent& event, const ClipboardCallbacks& clipboard);

  void beginTextInputSelection(Tree &tree, Font &font, NodeHandle handle,
                               Vec2 mousePosition, bool extendSelection);
  void updateTextInputSelection(Tree &tree, Font &font, NodeHandle handle,
                                Vec2 mousePosition, f32 deltaTime);
  void endTextInputSelection(Tree &tree, NodeHandle handle);

  void beginNumericInputDrag(Tree& tree, NodeHandle handle, Vec2 mousePosition);
  
  bool updateNumericInputDrag(Tree& tree, EventSystem& events, NodeHandle handle, Vec2 mousePosition);

  void endNumericInputDrag(Tree& tree, NodeHandle handle) noexcept;

  void commitNumericInput(Tree& tree, EventSystem& events, NodeHandle handle);

  void updateTextInputVisualState(Tree& tree, NodeHandle handle, f32 deltaTime);

  void setTextInputCharacterLimit(Tree& tree, NodeHandle handle, u32 maxCharacters);

  [[nodiscard]]
  u32 textInputCharacterLimit(const Tree& tree, NodeHandle handle) const noexcept;

private:
  [[nodiscard]]
  f32 clampSliderValue(const SliderState &state, f32 value) const noexcept;

  void ensureTextInputCaretVisible(Tree &tree, Font &font, NodeHandle handle);

  [[nodiscard]]
  f32 maxTextInputScrollX(const Tree &tree, Font &font, NodeHandle handle) const;

  [[nodiscard]]
  f64 normalizeNumericValue(const NumericInputState& state, f64 value) const noexcept;

  void syncNumericInputText(Tree& tree, NodeHandle handle);
  void adjustNumericInput(Tree& tree, EventSystem& events, NodeHandle handle, i32 direction);
  
};

} // namespace octogui