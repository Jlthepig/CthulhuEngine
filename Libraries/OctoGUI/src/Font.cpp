#include "OctoGui/Font.h"

#include "ft2build.h"
#include FT_FREETYPE_H

#include "hb-ft.h"
#include "hb.h"

#include <algorithm>
namespace octogui {
namespace {

constexpr usize maxCachedTexts = 512;
constexpr usize maxPersistentCachedTextBytes = 512;

bool isUtf8Continuation(char value) noexcept {
  const u8 byte = static_cast<u8>(value);
  return (byte & 0xC0u) == 0x80u;
}

FT_Library ensureFreeTypeLibrary() noexcept {
  static FT_Library library = nullptr;

  if (!library) {
    if (FT_Init_FreeType(&library) != 0) {
      library = nullptr;
    }
  }

  return library;
}
} // namespace

Font::~Font() { shutdown(); }

bool Font::load(const void *data, usize dataSize, u32 pixelSize,
                TextureBackend &backend) {
  shutdown();

  if (!data || dataSize == 0 || pixelSize == 0) {
    return false;
  }

  FT_Library library = ensureFreeTypeLibrary();

  if (!library) {
    return false;
  }

  fontData.assign(static_cast<const u8 *>(data),
                  static_cast<const u8 *>(data) + dataSize);

  FT_Face face = nullptr;

  if (FT_New_Memory_Face(library, fontData.data(),
                         static_cast<FT_Long>(fontData.size()), 0,
                         &face) != 0) {
    fontData.clear();
    return false;
  }

  FT_Select_Charmap(face, FT_ENCODING_UNICODE);

  if (FT_Set_Pixel_Sizes(face, 0, pixelSize) != 0) {
    FT_Done_Face(face);
    fontData.clear();
    return false;
  }

  hb_font_t *hbFontPtr = hb_ft_font_create(face, nullptr);

  if (!hbFontPtr) {
    FT_Done_Face(face);
    fontData.clear();
    return false;
  }

  hb_buffer_t *hbBufferPtr = hb_buffer_create();

  if (!hbBufferPtr) {
    hb_font_destroy(hbFontPtr);
    FT_Done_Face(face);
    fontData.clear();
    return false;
  }

  ftFace = face;
  hbFont = hbFontPtr;
  hbBuffer = hbBufferPtr;
  textureBackend = &backend;

  fontMetrics.ascent = static_cast<f32>(face->size->metrics.ascender) / 64.0f;
  fontMetrics.descent = static_cast<f32>(face->size->metrics.descender) / 64.0f;
  fontMetrics.lineHeight = static_cast<f32>(face->size->metrics.height) / 64.0f;

  atlasSize = 1024;
  atlasPadding = 1;
  cursorX = 0;
  cursorY = 0;
  shelfHeight = 0;
  atlasDirty = false;

  dirtyMinX = 0;
  dirtyMinY = 0;
  dirtyMaxX = 0;
  dirtyMaxY = 0;

  atlasPixels.assign(
      static_cast<usize>(atlasSize) * static_cast<usize>(atlasSize), 0);

  glyphs.clear();
  textCache.clear();
  transientTextKey.clear();
  transientText = CachedText{};

  textureHandle = backend.createTexture(atlasSize, atlasSize,
                                        atlasPixels.data(), TextureFormat::R8);

  if (!textureHandle.isValid()) {
    shutdown();
    return false;
  }

  return true;
}

void Font::shutdown() {
  if (textureBackend && textureHandle.isValid()) {
    textureBackend->destroyTexture(textureHandle);
  }

  textureHandle = TextureHandle{};
  textureBackend = nullptr;

  if (hbBuffer) {
    hb_buffer_destroy(static_cast<hb_buffer_t *>(hbBuffer));
    hbBuffer = nullptr;
  }
  if (hbFont) {
    hb_font_destroy(static_cast<hb_font_t *>(hbFont));
    hbFont = nullptr;
  }
  if (ftFace) {
    FT_Done_Face(static_cast<FT_Face>(ftFace));
    ftFace = nullptr;
  }

  textCache.clear();
  fontData.clear();
  atlasPixels.clear();
  glyphs.clear();
  transientTextKey.clear();

  transientText = CachedText{};
  fontMetrics = FontMetrics{};

  atlasSize = 1024;
  atlasPadding = 1;
  cursorX = 0;
  cursorY = 0;
  shelfHeight = 0;
  atlasDirty = false;
}

bool Font::isLoaded() const noexcept {
  return ftFace != nullptr && hbFont != nullptr && textureHandle.isValid();
}

FontMetrics Font::metrics() const noexcept { return fontMetrics; }
TextureHandle Font::texture() const noexcept { return textureHandle; }

Vec2 Font::measureText(std::string_view text) {
  if (!isLoaded() || text.empty()) {
    return Vec2{0.0f, fontMetrics.lineHeight};
  }

  return getShapedText(text).size;
}

f32 Font::caretX(std::string_view text, u32 byteOffset) {
  if (!isLoaded() || text.empty()) {
    return 0.0f;
  }

  const CachedText& cached =
      getShapedText(text);

  if (cached.carets.empty()) {
    return 0.0f;
  }

  const auto it =
      std::lower_bound(
          cached.carets.begin(),
          cached.carets.end(),
          byteOffset,
          [](const CaretStop& stop, u32 offset) {
            return stop.byteOffset < offset;
          });

  if (it == cached.carets.end()) {
    return cached.carets.back().x;
  }

  if (it->byteOffset == byteOffset ||
      it == cached.carets.begin()) {
    return it->x;
  }

  return std::prev(it)->x;
}

u32 Font::textOffsetAtX(std::string_view text, f32 x) {
  if (!isLoaded() || text.empty()) {
    return 0;
  }

  const CachedText& cached =
      getShapedText(text);

  if (cached.visualCarets.empty()) {
    return 0;
  }

  const auto it =
      std::lower_bound(
          cached.visualCarets.begin(),
          cached.visualCarets.end(),
          x,
          [](const CaretStop& stop, f32 position) {
            return stop.x < position;
          });

  if (it == cached.visualCarets.begin()) {
    return it->byteOffset;
  }

  if (it == cached.visualCarets.end()) {
    return cached.visualCarets.back().byteOffset;
  }

  const CaretStop& right = *it;
  const CaretStop& left = *std::prev(it);

  return (x - left.x) <=
                 (right.x - x)
             ? left.byteOffset
             : right.byteOffset;
}

void Font::drawText(DrawList &drawList, std::string_view text, Vec2 position,
                    Color color) {
  if (!isLoaded() || text.empty() || color.isTransparent()) {
    return;
  }

  const CachedText &cached = getShapedText(text);

  if (cached.glyphs.empty()) {
    return;
  }

  for (const ShapedGlyph &shapedGlyph : cached.glyphs) {
    ensureGlyph(shapedGlyph.glyphIndex);
  }

  flushAtlasIfNeeded();

  f32 penX = position.x + cached.originOffsetX;
  const f32 baseLineY = position.y + fontMetrics.ascent;

  for (const ShapedGlyph &shapedGlyph : cached.glyphs) {
    const auto it = glyphs.find(shapedGlyph.glyphIndex);

    if (it != glyphs.end()) {
      const FontGlyph &glyph = it->second;

      if (glyph.hasBitmap) {
        const f32 quadX = penX + shapedGlyph.xOffset + glyph.bearing.x;
        const f32 quadY = baseLineY - shapedGlyph.yOffset - glyph.bearing.y;

        drawList.addTexturedRect(Rect{quadX, quadY, glyph.size.x, glyph.size.y},
                                 glyph.uv, textureHandle, color);
      }
    }

    penX += shapedGlyph.xAdvance;
  }
}

void Font::drawTextClipped(DrawList& drawList, std::string_view text, Vec2 position, Color color, Rect clipRect) {
  if (!isLoaded() || text.empty() || color.isTransparent() || clipRect.isEmpty()) {
    return;
  }

  const CachedText& cached = getShapedText(text);

  if (cached.glyphs.empty()) {
    return;
  }

  if (position.y + fontMetrics.lineHeight <= clipRect.y ||
      position.y >= clipRect.maxY()) {
    return;
  }

  f32 penX = position.x + cached.originOffsetX;
  const f32 baselineY = position.y + fontMetrics.ascent;

  const f32 cullMargin =
      fontMetrics.lineHeight * 2.0f;

  for (const ShapedGlyph& shapedGlyph : cached.glyphs) {
    const f32 nextPenX =
        penX + shapedGlyph.xAdvance;

    const f32 advanceMin =
        penX < nextPenX ? penX : nextPenX;

    const f32 advanceMax =
        penX > nextPenX ? penX : nextPenX;

    const f32 approximateMin =
        advanceMin + shapedGlyph.xOffset - cullMargin;

    const f32 approximateMax =
        advanceMax + shapedGlyph.xOffset + cullMargin;

    if (approximateMax <= clipRect.x ||
        approximateMin >= clipRect.maxX()) {
      penX = nextPenX;
      continue;
    }

    FontGlyph& glyph =
        ensureGlyph(shapedGlyph.glyphIndex);

    if (glyph.hasBitmap) {
      const f32 quadX =
          penX + shapedGlyph.xOffset + glyph.bearing.x;

      const f32 quadY =
          baselineY - shapedGlyph.yOffset - glyph.bearing.y;

      const Rect quad{
          quadX,
          quadY,
          glyph.size.x,
          glyph.size.y
      };

      const bool visible =
          quad.maxX() > clipRect.x &&
          quad.x < clipRect.maxX() &&
          quad.maxY() > clipRect.y &&
          quad.y < clipRect.maxY();

      if (visible) {
        drawList.addTexturedRect(
            quad,
            glyph.uv,
            textureHandle,
            color);
      }
    }

    penX = nextPenX;
  }

  flushAtlasIfNeeded();
}

std::vector<Font::ShapedGlyph> Font::shapeText(std::string_view text,
                                               bool &outRightToLeft) {
  outRightToLeft = false;
  std::vector<ShapedGlyph> shaped;

  if (!hbFont || !hbBuffer || text.empty()) {
    return shaped;
  }

  hb_buffer_t *buffer = static_cast<hb_buffer_t *>(hbBuffer);

  hb_buffer_reset(buffer);

  hb_buffer_set_cluster_level(buffer,
                              HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);

  hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.length()), 0,
                     static_cast<int>(text.length()));

  hb_buffer_guess_segment_properties(buffer);

  const hb_direction_t direction = hb_buffer_get_direction(buffer);

  outRightToLeft = HB_DIRECTION_IS_BACKWARD(direction);

  hb_shape(static_cast<hb_font_t *>(hbFont), buffer, nullptr, 0);

  unsigned int glyphCount = 0;

  hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
  hb_glyph_position_t *glyphPositions =
      hb_buffer_get_glyph_positions(buffer, &glyphCount);

  shaped.reserve(glyphCount);

  for (unsigned int i = 0; i < glyphCount; ++i) {
    ShapedGlyph glyph;

    glyph.glyphIndex = glyphInfos[i].codepoint;
    glyph.cluster = glyphInfos[i].cluster;
    glyph.xOffset = static_cast<f32>(glyphPositions[i].x_offset) / 64.0f;
    glyph.yOffset = static_cast<f32>(glyphPositions[i].y_offset) / 64.0f;
    glyph.xAdvance = static_cast<f32>(glyphPositions[i].x_advance) / 64.0f;
    glyph.yAdvance = static_cast<f32>(glyphPositions[i].y_advance) / 64.0f;

    shaped.push_back(glyph);
  }

  return shaped;
}

void Font::buildCaretStops(std::string_view text, CachedText &cached) {
  cached.carets.clear();

  const u32 textSize = static_cast<u32>(text.size());

  if (text.empty()) {
    cached.carets.push_back(CaretStop{0, 0.0f});
    return;
  }

  struct ClusterSpan {
    u32 byteOffset = 0;
    f32 startX = 0.0f;
    f32 endX = 0.0f;
  };

  std::vector<ClusterSpan> spans;
  spans.reserve(cached.glyphs.size());

  f32 penX = cached.originOffsetX;

  for (const ShapedGlyph &glyph : cached.glyphs) {
    if (spans.empty() || spans.back().byteOffset != glyph.cluster) {
      spans.push_back(ClusterSpan{glyph.cluster, penX, penX});
    }

    penX += glyph.xAdvance;
    spans.back().endX = penX;
  }

  if (spans.empty()) {
    cached.carets.push_back(CaretStop{0, 0.0f});
    cached.carets.push_back(CaretStop{textSize, 0.0f});
    return;
  }

  std::sort(spans.begin(), spans.end(),
            [](const ClusterSpan &a, const ClusterSpan &b) {
              return a.byteOffset < b.byteOffset;
            });

  std::vector<u32> boundaries;
  boundaries.reserve(text.size() + 1);

  boundaries.push_back(0);

  u32 offset = 0;

  while (offset < textSize) {
    ++offset;

    while (offset < textSize && isUtf8Continuation(text[offset])) {
      ++offset;
    }

    boundaries.push_back(offset);
  }

  for (usize spanIndex = 0; spanIndex < spans.size(); ++spanIndex) {
    const ClusterSpan &span = spans[spanIndex];

    u32 clusterStart = span.byteOffset;

    if (clusterStart > textSize) {
      clusterStart = textSize;
    }

    u32 clusterEnd = textSize;

    if (spanIndex + 1 < spans.size()) {
      clusterEnd = spans[spanIndex + 1].byteOffset;

      if (clusterEnd > textSize) {
        clusterEnd = textSize;
      }
    }

    if (clusterEnd < clusterStart) {
      continue;
    }

    const auto first =
        std::lower_bound(boundaries.begin(), boundaries.end(), clusterStart);

    const auto last =
        std::upper_bound(boundaries.begin(), boundaries.end(), clusterEnd);

    const usize boundaryCount = static_cast<usize>(last - first);

    if (boundaryCount == 0) {
      continue;
    }

    const f32 leadingX = cached.rightToLeft ? span.endX : span.startX;

    const f32 trailingX = cached.rightToLeft ? span.startX : span.endX;

    for (usize i = 0; i < boundaryCount; ++i) {
      const u32 boundary = *(first + i);

      f32 t = 0.0f;

      if (boundaryCount > 1) {
        t = static_cast<f32>(i) / static_cast<f32>(boundaryCount - 1);
      }

      const f32 x = leadingX + (trailingX - leadingX) * t;

      if (!cached.carets.empty() &&
          cached.carets.back().byteOffset == boundary) {
        cached.carets.back().x = x;
      } else {
        cached.carets.push_back(CaretStop{boundary, x});
      }
    }
  }

  if (cached.carets.empty()) {
    cached.carets.push_back(CaretStop{0, 0.0f});
    cached.carets.push_back(CaretStop{textSize, cached.size.x});
  }

  std::sort(
      cached.carets.begin(),
      cached.carets.end(),
      [](const CaretStop& lhs, const CaretStop& rhs) {
        return lhs.byteOffset < rhs.byteOffset;
      });

  cached.visualCarets =
      cached.carets;

  std::sort(
      cached.visualCarets.begin(),
      cached.visualCarets.end(),
      [](const CaretStop& lhs, const CaretStop& rhs) {
        if (lhs.x == rhs.x) {
          return lhs.byteOffset <
                 rhs.byteOffset;
        }

        return lhs.x < rhs.x;
      });
}

const Font::CachedText &Font::getShapedText(std::string_view text) {
  const bool persistent =
    text.size() <= maxPersistentCachedTextBytes;

  if (persistent) {
    const auto existing = textCache.find(text);

    if (existing != textCache.end()) {
      return existing->second;
    }
  } else if (std::string_view{transientTextKey} == text) {
    return transientText;
  }

  CachedText cached;

  cached.glyphs = shapeText(text, cached.rightToLeft);

  f32 penX = 0.0f;
  f32 minPenX = 0.0f;
  f32 maxPenX = 0.0f;

  for (const ShapedGlyph &glyph : cached.glyphs) {
    penX += glyph.xAdvance;

    if (penX < minPenX) {
      minPenX = penX;
    }

    if (penX > maxPenX) {
      maxPenX = penX;
    }
  }

  cached.originOffsetX = -minPenX;

  cached.size = Vec2{maxPenX - minPenX, fontMetrics.lineHeight};

  buildCaretStops(text, cached);

  if (!persistent) {
    transientTextKey.assign(text.data(), text.size());
    transientText = std::move(cached);

    return transientText;
  }

  if (textCache.size() >= maxCachedTexts) {
    textCache.erase(textCache.begin());
  }

  const auto inserted = textCache.emplace(std::string(text), std::move(cached));

  return inserted.first->second;
}

FontGlyph &Font::ensureGlyph(u32 glyphIndex) {
  const auto existing = glyphs.find(glyphIndex);

  if (existing != glyphs.end()) {
    return existing->second;
  }

  FontGlyph glyph;

  FT_Face face = static_cast<FT_Face>(ftFace);

  if (face && FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER) == 0) {
    FT_GlyphSlot slot = face->glyph;

    glyph.advance = static_cast<f32>(slot->advance.x) / 65536.0f;

    if (slot->format == FT_GLYPH_FORMAT_BITMAP && slot->bitmap.buffer &&
        slot->bitmap.width > 0 && slot->bitmap.rows > 0 &&
        slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
      const u32 width = static_cast<u32>(slot->bitmap.width);
      const u32 height = static_cast<u32>(slot->bitmap.rows);

      u32 atlasX = 0;
      u32 atlasY = 0;

      if (allocateAtlasRect(width, height, atlasX, atlasY)) {
        copyBitmapToAtlas(atlasX, atlasY, width, height, slot->bitmap.pitch,
                          slot->bitmap.buffer);
      }

      glyph.hasBitmap = true;
      glyph.size = Vec2{static_cast<f32>(width), static_cast<f32>(height)};

      glyph.bearing = Vec2{static_cast<f32>(slot->bitmap_left),
                           static_cast<f32>(slot->bitmap_top)};

      glyph.uv = Rect{
          static_cast<f32>(atlasX + atlasPadding) / static_cast<f32>(atlasSize),
          static_cast<f32>(atlasY + atlasPadding) / static_cast<f32>(atlasSize),
          static_cast<f32>(width) / static_cast<f32>(atlasSize),
          static_cast<f32>(height) / static_cast<f32>(atlasSize)};

      markAtlasDirty(atlasX + atlasPadding, atlasY + atlasPadding, width,
                     height);
    }
  }

  const auto inserted = glyphs.emplace(glyphIndex, glyph);
  return inserted.first->second;
}

bool Font::allocateAtlasRect(u32 width, u32 height, u32 &outX, u32 &outY) {
  const u32 blockWidth = width + atlasPadding * 2;
  const u32 blockHeight = height + atlasPadding * 2;

  if (blockWidth > atlasSize || blockHeight > atlasSize) {
    return false;
  }

  if (cursorX + blockWidth > atlasSize) {
    cursorX = 0;
    cursorY += shelfHeight;
    shelfHeight = 0;
  }

  if (cursorY + blockHeight > atlasSize) {
    return false;
  }

  outX = cursorX;
  outY = cursorY;

  cursorX += blockWidth;

  if (blockHeight > shelfHeight) {
    shelfHeight = blockHeight;
  }

  return true;
}

void Font::copyBitmapToAtlas(u32 x, u32 y, u32 width, u32 height, int pitch,
                             const unsigned char *buffer) {
  if (!buffer) {
    return;
  }

  for (u32 row = 0; row < height; ++row) {
    const unsigned char *sourceRow;

    if (pitch >= 0) {
      sourceRow = buffer + static_cast<usize>(row) * static_cast<usize>(pitch);
    } else {
      sourceRow = buffer + static_cast<usize>(height - 1 - row) *
                               static_cast<usize>(-pitch);
    }

    const usize destinationY = static_cast<usize>(y + atlasPadding + row);

    for (u32 column = 0; column < width; column++) {
      const usize destinationIndex =
          destinationY * static_cast<usize>(atlasSize) +
          static_cast<usize>(x + atlasPadding + column);

      atlasPixels[destinationIndex] = sourceRow[column];
    }
  }
}

void Font::markAtlasDirty(u32 x, u32 y, u32 width, u32 height) noexcept {
  if (width == 0 || height == 0) {
    return;
  }

  const u32 maxX = x + width;
  const u32 maxY = y + height;

  if (!atlasDirty) {
    dirtyMinX = x;
    dirtyMinY = y;
    dirtyMaxX = maxX;
    dirtyMaxY = maxY;

    atlasDirty = true;
    return;
  }

  if (x < dirtyMinX) {
    dirtyMinX = x;
  }
  if (y < dirtyMinY) {
    dirtyMinY = y;
  }

  if (maxX > dirtyMaxX) {
    dirtyMaxX = maxX;
  }
  if (maxY > dirtyMaxY) {
    dirtyMaxY = maxY;
  }
}

void Font::flushAtlasIfNeeded() {
  if (!atlasDirty || !textureBackend || !textureHandle.isValid()) {
    return;
  }

  const u32 width = dirtyMaxX - dirtyMinX;
  const u32 height = dirtyMaxY - dirtyMinY;

  const usize offset =
      static_cast<usize>(dirtyMinY) * static_cast<usize>(atlasSize) +
      static_cast<usize>(dirtyMinX);

  textureBackend->updateTextureRegion(
      textureHandle, dirtyMinX, dirtyMinY, width, height,
      atlasPixels.data() + offset, atlasSize, TextureFormat::R8);
  atlasDirty = false;
}
} // namespace octogui