#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "OctoGui/Color.h"
#include "OctoGui/DrawList.h"
#include "OctoGui/Rect.h"
#include "OctoGui/TextureBackend.h"
#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {

struct FontMetrics {
  f32 ascent = 0.0f;
  f32 descent = 0.0f;
  f32 lineHeight = 0.0f;
};

struct FontGlyph {
  Rect uv = Rect::zero();
  Vec2 size = Vec2::zero();
  Vec2 bearing = Vec2::zero();
  f32 advance = 0.0f;
  bool hasBitmap = false;
};

class Font {

public:
  Font() = default;

  ~Font();

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  Font(Font &&) = delete;
  Font &operator=(Font &&) = delete;

  bool load(const void *data, usize dataSize, u32 pixelSize,
            TextureBackend &backend);

  void shutdown();

  [[nodiscard]]
  bool isLoaded() const noexcept;

  [[nodiscard]]
  FontMetrics metrics() const noexcept;

  [[nodiscard]]
  TextureHandle texture() const noexcept;

  Vec2 measureText(std::string_view text);

  [[nodiscard]]
  f32 caretX(std::string_view text, u32 byteOffset);

  [[nodiscard]]
  u32 textOffsetAtX(std::string_view text, f32 x);

  void drawText(DrawList &drawList, std::string_view text, Vec2 position,
                Color color);

  void drawTextClipped(DrawList& drawList, std::string_view text, Vec2 position, Color color, Rect clipRect);

private:
  struct ShapedGlyph {
    u32 glyphIndex = 0;
    u32 cluster = 0;

    f32 xOffset = 0.0f;
    f32 yOffset = 0.0f;
    f32 xAdvance = 0.0f;
    f32 yAdvance = 0.0f;
  };
  struct CaretStop {
    u32 byteOffset = 0;
    f32 x = 0.0f;
  };
  struct CachedText {
    std::vector<ShapedGlyph> glyphs;
    std::vector<CaretStop> carets;
    std::vector<CaretStop> visualCarets;

    Vec2 size = Vec2::zero();

    f32 originOffsetX = 0.0f;
    bool rightToLeft = false;
  };

  struct StringHash {
    using is_transparent = void;

    size_t operator()(std::string_view value) const {
      return std::hash<std::string_view>{}(value);
    }
  };

  std::vector<ShapedGlyph> shapeText(std::string_view text,
                                     bool &outRightToLeft);

  void buildCaretStops(std::string_view text, CachedText &cached);

  FontGlyph &ensureGlyph(u32 glyphIndex);

  bool allocateAtlasRect(u32 width, u32 height, u32 &outX, u32 &outY);

  void copyBitmapToAtlas(u32 x, u32 y, u32 width, u32 height, int pitch,
                         const unsigned char *buffer);

  void flushAtlasIfNeeded();

  void markAtlasDirty(u32 x, u32 y, u32 width, u32 height) noexcept;

  const CachedText &getShapedText(std::string_view text);

  std::vector<u8> fontData;

  void *ftFace = nullptr;
  void *hbFont = nullptr;
  void *hbBuffer = nullptr;

  TextureBackend *textureBackend = nullptr;
  TextureHandle textureHandle{};

  std::vector<u8> atlasPixels;
  std::unordered_map<u32, FontGlyph> glyphs;
  std::unordered_map<std::string, CachedText, StringHash, std::equal_to<>>
      textCache;

  std::string transientTextKey;
  CachedText transientText;

  FontMetrics fontMetrics{};

  u32 atlasSize = 1024;
  u32 atlasPadding = 1;

  u32 cursorX = 0;
  u32 cursorY = 0;
  u32 shelfHeight = 0;

  bool atlasDirty = false;
  u32 dirtyMinX = 0;
  u32 dirtyMinY = 0;
  u32 dirtyMaxX = 0;
  u32 dirtyMaxY = 0;
};
} // namespace octogui
