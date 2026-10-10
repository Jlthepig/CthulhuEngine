#pragma once

#include "OctoGui/Tree.h"

namespace octogui {

namespace detail {

struct LayoutTarget {
  Tree *tree = nullptr;
  NodeHandle handle{};

  [[nodiscard]]
  LayoutStyle *style() noexcept {
    if (!tree || !tree->isValid(handle)) {
      return nullptr;
    }

    return tree->layoutStyle(handle);
  }

  const LayoutStyle *style() const noexcept {
    if (!tree || !tree->isValid(handle)) {
      return nullptr;
    }

    return static_cast<const Tree &>(*tree).layoutStyle(handle);
  }

  void dirty() noexcept {
    if (tree) {
      tree->markLayoutDirty(handle);
    }
  }
};

template <typename T> struct LayoutProperty {
  LayoutTarget target;
  T LayoutStyle::*field;

  LayoutProperty &operator=(const T &value) noexcept {
    LayoutStyle *style = target.style();

    if (!style) {
      return *this;
    }

    if (style->*field != value) {
      style->*field = value;
      target.dirty();
    }

    return *this;
  }

  LayoutProperty &operator=(const LayoutProperty &other) noexcept {
    return *this = static_cast<T>(other);
  }

  operator T() const noexcept {
    if (const LayoutStyle *style = target.style()) {
      return style->*field;
    }

    return T{};
  }
};

} // namespace detail

class LayoutProxy {
public:
  detail::LayoutProperty<LayoutMode> mode;
  detail::LayoutProperty<Alignment> horizontalAlignment;
  detail::LayoutProperty<Alignment> verticalAlignment;

  detail::LayoutProperty<f32> gap;
  detail::LayoutProperty<Padding> padding;

  detail::LayoutProperty<Vec2> minSize;
  detail::LayoutProperty<Vec2> preferredSize;
  detail::LayoutProperty<Vec2> maxSize;

  detail::LayoutProperty<bool> fitContentY;
  detail::LayoutProperty<bool> clip;
  detail::LayoutProperty<bool> scrollY;

  explicit LayoutProxy(detail::LayoutTarget target) noexcept
      : mode{target, &LayoutStyle::mode},
        horizontalAlignment{target, &LayoutStyle::horizontalAlignment},
        verticalAlignment{target, &LayoutStyle::verticalAlignment},
        gap{target, &LayoutStyle::gap}, padding{target, &LayoutStyle::padding},
        minSize{target, &LayoutStyle::minSize},
        preferredSize{target, &LayoutStyle::preferredSize},
        maxSize{target, &LayoutStyle::maxSize},
        fitContentY{target, &LayoutStyle::fitContentY},
        clip{target, &LayoutStyle::clip},
        scrollY{target, &LayoutStyle::scrollY} {}
};
} // namespace octogui