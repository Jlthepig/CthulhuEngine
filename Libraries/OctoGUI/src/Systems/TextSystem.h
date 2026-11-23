#pragma once

#include <string_view>

#include "OctoGui/Font.h"
#include "OctoGui/TextureBackend.h"
#include "OctoGui/Tree.h"
namespace octogui {
class TextSystem {
public:
  void setTextureBackend(TextureBackend *backend) noexcept;

  bool loadFont(FontSlot slot, std::string_view path, u32 pixelSize);

  [[nodiscard]]
  Font &font(FontSlot slot = FontSlot::Default) noexcept;

  [[nodiscard]]
  const Font &font(FontSlot slot = FontSlot::Default) const noexcept;

  // The font a node's own text is measured with. Falls back to the default
  // font when the node's slot has nothing loaded.
  [[nodiscard]]
  Font &fontFor(const Tree &tree, NodeHandle node) noexcept;

  void updateLayoutHints(Tree &tree, NodeHandle root);

private:
  TextureBackend *textureBackend = nullptr;
  std::array<Font, FontSlotCount> fonts;
};

} // namespace octogui