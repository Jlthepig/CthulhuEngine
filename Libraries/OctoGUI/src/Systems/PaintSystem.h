#pragma once

#include <vector>

#include "OctoGui/DrawList.h"
#include "OctoGui/Font.h"
#include "OctoGui/Tree.h"
#include "OctoGui/Types.h"
namespace octogui {

struct InteractionState;
class IconAtlas;
class PaintSystem {
public:
  [[nodiscard]]
  DrawList &drawList() noexcept;

  [[nodiscard]]
  const DrawList &drawList() const noexcept;

  void beginFrame() noexcept;

  void paint(Tree &tree, NodeHandle root, const InteractionState &interaction,
             Font *font, const IconAtlas& icons);

private:
  struct PaintCacheEntry {
    u32 generation = 0;
    DrawList commands;
  };

  struct ComboBoxOverlay {
    NodeHandle handle{};
  };

  std::vector<PaintCacheEntry> paintCache;
  std::vector<NodeHandle> zOrderScratch;
  std::vector<ComboBoxOverlay> comboBoxOverlays;

  DrawList drawListData;

  [[nodiscard]]
  DrawList &paintCacheFor(NodeHandle handle);

  void paintNode(Tree &tree, DrawList &drawList, NodeHandle node, const InteractionState &interaction, Font *font, const IconAtlas& icons, Vec2 appendOffset);

  void paintNodeText(const Tree &tree, DrawList &drawList, Font &font, NodeHandle node, Rect nodeRect, Color color, const InteractionState& interaction);

  void paintSlider(const Tree &tree, DrawList &drawList, NodeHandle node, const InteractionState &interaction, Rect rect);

  void paintProgressBar(const Tree &tree, DrawList &drawList, NodeHandle node, Font *font, Rect rect);

  void paintCheckbox(const Tree &tree, DrawList &drawList, NodeHandle node, const InteractionState &interaction, Font *font, Rect rect);

  void paintRadioButton(const Tree &tree, DrawList &drawList, NodeHandle node, const InteractionState &interaction, Font *font, Rect rect);
                        
  void paintSeparator(const Tree& tree, DrawList& drawList, NodeHandle node, const InteractionState& interaction, Rect rect);

  void paintSplitContainerDivider(const Tree& tree, DrawList& drawList,
                                  NodeHandle node,
                                  const InteractionState& interaction,
                                  Rect rect);
                      
  void paintComboBoxClosed(const Tree& tree, DrawList& drawList, Font& font, NodeHandle node, Rect rect, Color textColor);
  
  void trackComboBoxOverlay(NodeHandle handle);

  void pruneComboBoxOverlays(const Tree& tree);

  void paintComboBoxOverlays(const Tree& tree, NodeHandle root, const InteractionState& interaction, Font* font);

  void paintComboBoxPopup(const Tree& tree, DrawList& drawList, Font* font, NodeHandle node, Rect anchorRect, Rect viewport);
  
  void paintCollapsibleSectionIndicator(const Tree& tree, DrawList& drawList, NodeHandle node, Rect rect, Color color);
  
  void paintTreeView(Tree& tree, DrawList& drawList, Font* font, NodeHandle node, Rect rect);
  
  void paintIcon(const Tree& tree, DrawList& drawList, NodeHandle node, const IconAtlas& icons, Rect rect);
};

} // namespace octogui