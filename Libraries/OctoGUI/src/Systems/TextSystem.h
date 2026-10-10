#pragma once

#include <string_view>

#include "OctoGui/Font.h"
#include "OctoGui/TextureBackend.h"
#include "OctoGui/Tree.h"
namespace octogui {
class TextSystem {
public:
  void setTextureBackend(TextureBackend *backend) noexcept;

  bool loadDefaultFont(std::string_view path, u32 pixelSize = 18);

  [[nodiscard]]
  Font &font() noexcept;

  [[nodiscard]]
  const Font &font() const noexcept;

  void updateLayoutHints(Tree &tree, NodeHandle root);

private:
  TextureBackend *textureBackend = nullptr;
  Font defaultFont;
};

} // namespace octogui