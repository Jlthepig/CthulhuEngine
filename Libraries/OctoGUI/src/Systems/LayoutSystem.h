#pragma once

#include "OctoGui/Input.h"
#include "OctoGui/Tree.h"
namespace octogui {

class TextSystem;
class LayoutSystem {
public:
  void update(Tree &tree, NodeHandle root, const InputState &input,
              TextSystem &text);
};

} // namespace octogui