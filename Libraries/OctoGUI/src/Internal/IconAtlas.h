#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "OctoGui/Rect.h"
#include "OctoGui/TextureBackend.h"

namespace octogui {

struct ResolvedIcon {
  TextureHandle texture{};
  Rect uv{};
};

struct CustomIconData {
  TextureHandle texture{};
  IconColorMode colorMode = IconColorMode::Tintable;
};

class IconAtlas {
public:
  IconAtlas() = default;
  ~IconAtlas();

  IconAtlas(const IconAtlas&) = delete;
  IconAtlas& operator=(const IconAtlas&) = delete;

  void setTextureBackend(TextureBackend* backend) noexcept;

  bool loadDefault(std::string_view path);
  bool loadCustom(std::string_view name,
                  std::string_view path,
                  IconColorMode colorMode);

  void shutdown() noexcept;

  [[nodiscard]]
  bool isLoaded() const noexcept;

  [[nodiscard]]
  TextureHandle texture() const noexcept;

  [[nodiscard]]
  bool isImageColor(std::string_view name) const noexcept;

  [[nodiscard]]
  bool lookup(
      std::string_view name,
      ResolvedIcon& icon) const noexcept;

private:
  TextureBackend* textureBackend = nullptr;
  TextureHandle textureHandle{};
  std::unordered_map<std::string, CustomIconData> customIcons;
};

} // namespace octogui