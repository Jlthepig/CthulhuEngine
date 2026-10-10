#include <array>
#include <string>
#include <vector>

#include "Internal/IconAtlas.h"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
// STB_IMAGE_STATIC leaves the stb functions OctoGUI does not use unreferenced
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define STBI_ONLY_PNG
// keep stb_image private to OctoGUI so it never clashes with the host app's copy
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace octogui {

namespace {

inline constexpr u32 DefaultIconAtlasColumns = 8;
inline constexpr u32 DefaultIconAtlasRows = 5;
inline constexpr u32 DefaultIconSize = 24;

inline constexpr u32 DefaultIconAtlasWidth =
    DefaultIconAtlasColumns * DefaultIconSize;

inline constexpr u32 DefaultIconAtlasHeight =
    DefaultIconAtlasRows * DefaultIconSize;

inline constexpr std::array<std::string_view, 40> DefaultIconNames{
    "folder",
    "folder-open",
    "file",
    "file-code",
    "settings",
    "search",
    "plus",
    "minus",

    "close",
    "check",
    "chevron-left",
    "chevron-right",
    "chevron-up",
    "chevron-down",
    "arrow-left",
    "arrow-right",

    "refresh",
    "save",
    "trash",
    "copy",
    "clipboard",
    "terminal",
    "code",
    "play",

    "stop",
    "pause",
    "eye",
    "eye-off",
    "lock",
    "unlock",
    "menu",
    "more-horizontal",

    "info",
    "warning",
    "home",
    "box",
    "sidebar-left",
    "sidebar-right",
    "sliders",
    "tool"
};

}

IconAtlas::~IconAtlas() {
  shutdown();
}

void IconAtlas::setTextureBackend(TextureBackend* backend) noexcept {
  if (textureBackend == backend) {return;}

  shutdown();
  textureBackend = backend;
}

void IconAtlas::shutdown() noexcept {
  if (textureBackend) {
    for (auto& [name, data] : customIcons) {
      if (data.texture.isValid()) {
        textureBackend->destroyTexture(data.texture);
      }
    }

    if (textureHandle.isValid()) {
      textureBackend->destroyTexture(textureHandle);
    }
  }

  customIcons.clear();
  textureHandle = {};
}

bool IconAtlas::isLoaded() const noexcept {
  return textureHandle.isValid();
}

TextureHandle IconAtlas::texture() const noexcept {
  return textureHandle;
}

bool IconAtlas::isImageColor(std::string_view name) const noexcept {
  const auto custom = customIcons.find(std::string{name});

  if (custom == customIcons.end()) {
    return false;
  }

  return custom->second.colorMode == IconColorMode::Original;
}

bool IconAtlas::loadDefault(std::string_view path) {
  if (!textureBackend || path.empty()) {
    return false;
  }

  const std::string filePath{path};

  int width = 0;
  int height = 0;
  int channels = 0;

  stbi_uc* pixels =
      stbi_load(
          filePath.c_str(),
          &width,
          &height,
          &channels,
          4);

  if (!pixels) {
    return false;
  }

  const bool validSize =
      width == static_cast<int>(DefaultIconAtlasWidth) &&
      height == static_cast<int>(DefaultIconAtlasHeight);

  if (!validSize) {
    stbi_image_free(pixels);
    return false;
  }

  std::vector<u8> alpha(
      static_cast<usize>(DefaultIconAtlasWidth) *
      static_cast<usize>(DefaultIconAtlasHeight));

  for (usize i = 0; i < alpha.size(); ++i) {
    alpha[i] =
        static_cast<u8>(
            pixels[i * 4 + 3]);
  }

  stbi_image_free(pixels);

  if (textureHandle.isValid()) {
    textureBackend->updateTextureRegion(
        textureHandle,
        0,
        0,
        DefaultIconAtlasWidth,
        DefaultIconAtlasHeight,
        alpha.data(),
        DefaultIconAtlasWidth,
        TextureFormat::R8);

    return true;
  }

  textureHandle =
      textureBackend->createTexture(
          DefaultIconAtlasWidth,
          DefaultIconAtlasHeight,
          alpha.data(),
          TextureFormat::R8);

  return textureHandle.isValid();
}

bool IconAtlas::loadCustom(
  std::string_view name,
  std::string_view path,
  IconColorMode colorMode) {
  if (!textureBackend ||
      name.empty() ||
      path.empty()) {
    return false;
  }

  const std::string filePath{path.data(), path.size()};

  int width = 0;
  int height = 0;
  int channels = 0;

  stbi_uc* pixels =
      stbi_load(
          filePath.c_str(),
          &width,
          &height,
          &channels,
          STBI_rgb_alpha);

  if (!pixels) {
    return false;
  }

  if (width <= 0 ||
      height <= 0 ||
      width != height) {
    stbi_image_free(pixels);
    return false;
  }

  TextureHandle newTexture{};

  if (colorMode == IconColorMode::Original) {
    newTexture =
        textureBackend->createTexture(
            static_cast<u32>(width),
            static_cast<u32>(height),
            pixels,
            TextureFormat::RGBA8);
  } else {
    const usize pixelCount =
        static_cast<usize>(width) *
        static_cast<usize>(height);

    std::vector<u8> alpha(pixelCount);

    for (usize i = 0;
         i < pixelCount;
         ++i) {
      alpha[i] =
          pixels[i * 4 + 3];
    }

    newTexture =
        textureBackend->createTexture(
            static_cast<u32>(width),
            static_cast<u32>(height),
            alpha.data(),
            TextureFormat::R8);
  }

  stbi_image_free(pixels);

  if (!newTexture.isValid()) {
    return false;
  }

  const std::string iconName{ name.data(), name.size() };

  auto existing = customIcons.find(iconName);

  if (existing != customIcons.end()) {
    if (existing->second.texture.isValid()) {
      textureBackend->destroyTexture(existing->second.texture);
    }

    existing->second = CustomIconData{newTexture, colorMode};
  } else {
    customIcons.emplace(iconName, CustomIconData{newTexture, colorMode});
  }

  return true;
}

bool IconAtlas::lookup(
    std::string_view name,
    ResolvedIcon& icon) const noexcept {
  icon = {};

  const auto custom = customIcons.find(std::string{name});

  if (custom != customIcons.end() && custom->second.texture.isValid()) {
    icon.texture = custom->second.texture;
    icon.uv = Rect{0.0f, 0.0f, 1.0f, 1.0f};
    return true;
  }

  if (!textureHandle.isValid()) {
    return false;
  }

  for (usize i = 0; i < DefaultIconNames.size(); ++i) {
    if (DefaultIconNames[i] != name) {
      continue;
    }

    const u32 index =
        static_cast<u32>(i);

    const u32 column =
        index % DefaultIconAtlasColumns;

    const u32 row =
        index / DefaultIconAtlasColumns;

    const f32 u =
        static_cast<f32>(column) /
        static_cast<f32>(DefaultIconAtlasColumns);

    const f32 v =
        static_cast<f32>(row) /
        static_cast<f32>(DefaultIconAtlasRows);

    const f32 width =
        1.0f /
        static_cast<f32>(DefaultIconAtlasColumns);

    const f32 height =
        1.0f /
        static_cast<f32>(DefaultIconAtlasRows);

    icon.texture = textureHandle;
    icon.uv = Rect{u, v, width, height};
    return true;
  }

  return false;
}

}