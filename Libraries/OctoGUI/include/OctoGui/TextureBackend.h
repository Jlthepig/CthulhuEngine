#pragma once

#include "OctoGui/DrawList.h"
#include "OctoGui/Types.h"

namespace octogui {

class TextureBackend {

public:
  virtual ~TextureBackend() = default;

  virtual TextureHandle createTexture(u32 width, u32 height, const void *pixels,
                                      TextureFormat format) = 0;

  virtual void updateTexture(TextureHandle handle, u32 width, u32 height,
                             const void *pixels, TextureFormat format) = 0;
  virtual void updateTextureRegion(TextureHandle, u32 x, u32 y, u32 width,
                                   u32 height, const void *pixels,
                                   u32 rowStrideBytes,
                                   TextureFormat format) = 0;

  virtual void destroyTexture(TextureHandle handle) = 0;
};
} // namespace octogui