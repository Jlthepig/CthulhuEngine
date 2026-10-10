#pragma once

#include <algorithm>
#include <vector>

#include "OctoGui/Tree.h"

namespace octogui::detail {

struct ZOrderRange {
  usize begin = 0;
  usize end = 0;
};

inline ZOrderRange appendChildrenByZ(const Tree &tree, NodeHandle parent,
                                     std::vector<NodeHandle> &scratch) {
  const usize begin = scratch.size();

  bool needsSort = false;
  bool hasPrevious = false;
  i32 previousLayer = 0;

  NodeHandle child = tree.firstChild(parent);

  while (tree.isValid(child)) {
    const i32 layer = tree.zLayer(child);
    if (hasPrevious && layer < previousLayer) {
      needsSort = true;
    }
    hasPrevious = true;
    previousLayer = layer;

    scratch.push_back(child);
    child = tree.nextSibling(child);
  }

  const usize end = scratch.size();

  if (needsSort) {
    std::stable_sort(scratch.begin() + begin, scratch.begin() + end,
                     [&tree](NodeHandle a, NodeHandle b) {
                       return tree.zLayer(a) < tree.zLayer(b);
                     });
  }

  return ZOrderRange{begin, end};
}

} // namespace octogui::detail