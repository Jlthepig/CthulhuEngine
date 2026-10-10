#pragma once

#include "OctoGui/Style.h"
#include "OctoGui/Tree.h"
namespace octogui {
class StyleSystem {
public:
  [[nodiscard]]
  Theme &theme() noexcept;

  [[nodiscard]]
  const Theme &theme() const noexcept;

  void setTheme(Tree &tree, NodeHandle root, const Theme &theme);

  [[nodiscard]]
  VisualStyle &defaultVisualStyle(NodeType type) noexcept;

  [[nodiscard]]
  const VisualStyle &defaultVisualStyle(NodeType type) const noexcept;

  void resetDefaults() noexcept;

  void applyDefault(Tree &tree, NodeHandle handle, NodeType type);


private:
  struct DefaultStyles {
    VisualStyle root;
    VisualStyle container;
    VisualStyle button;
    VisualStyle label;
    VisualStyle checkbox;
    VisualStyle slider;
    VisualStyle radioButton;
    VisualStyle textInput;
    VisualStyle separator;
    VisualStyle custom;
  };

  Theme themeData = defaultTheme();
  DefaultStyles defaultStyles;
};

} // namespace octogui