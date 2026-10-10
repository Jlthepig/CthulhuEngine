#include "Systems/LayoutSystem.h"

#include "Systems/TextSystem.h"

#include "OctoGui/Layout.h"

namespace octogui {

void LayoutSystem::update(Tree &tree, NodeHandle root, const InputState &input,
                          TextSystem &text) {
  if (!tree.isValid(root)) {
    return;
  }

  text.updateLayoutHints(tree, root);

  LayoutInput layoutInput;
  layoutInput.availableSize = input.displaySize;
  layoutInput.dpiScale = input.dpiScale;

  solveLayout(tree, root, layoutInput);
}

} // namespace octogui