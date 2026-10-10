#pragma once

#include <span>
#include <vector>

#include "OctoGui/Color.h"
#include "OctoGui/Gradient.h"
#include "OctoGui/Handle.h"
#include "OctoGui/Rect.h"
#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {
struct TextureTag {};

using TextureHandle = Handle<TextureTag>;

enum class TextureFormat : u8 { RGBA8, R8 };

enum class DrawCommandType : u8 {
  Nop,
  PushClip,
  PopClip,
  Rect,
  RoundedRect,
  GradientRect,
  Shadow,
  TexturedRect,
  Line
};

struct DrawCommand {
  DrawCommandType type = DrawCommandType::Nop;

  Rect rect = Rect::zero();
  Rect uv{0.0f, 0.0f, 1.0f, 1.0f};
  Color color = Color::white();
  LinearGradient gradient{};
  TextureHandle texture{};

  f32 radius = 0.0f;
  f32 thickness = 0.0f;

  Vec2 shadowOffset{};
  f32 shadowBlur = 0.0f;
  f32 shadowSpread = 0.0f;

  Vec2 start{};
  Vec2 end{};
};

class DrawList {
public:
  void clear() noexcept;

  void reserve(usize count);

  [[nodiscard]]
  usize size() const noexcept {
    return commands.size();
  }

  [[nodiscard]]
  bool empty() const noexcept {
    return commands.empty();
  }

  [[nodiscard]]
  std::span<const DrawCommand> span() const noexcept {
    return commands;
  }

  [[nodiscard]]
  const std::vector<DrawCommand> &all() const noexcept {
    return commands;
  }

  [[nodiscard]]
  const DrawCommand &operator[](usize index) const noexcept {
    return commands[index];
  }

  void pushClip(Rect clipRect);

  void popClip();

  void addRectFilled(Rect rect, Color color);

  void addRectOutline(Rect rect, Color color, f32 thickness);

  void addImage(TextureHandle texture, Rect rect, Color tint = Color::white());

  void addTexturedRect(Rect rect, Rect uv, TextureHandle texture,
                       Color color = Color::white());

  void addRoundedRectFilled(Rect rect, Color color, f32 radius);

  void addRoundedRectOutline(Rect rect, Color color, f32 radius, f32 thickness);

  void addGradientRect(Rect rect, const LinearGradient &gradient);
  void addGradientRoundedRect(Rect rect, const LinearGradient &gradient,
                              f32 radius);

  void addBoxShadow(Rect rect, Color color, f32 radius, Vec2 offset, f32 blur,
                    f32 spread = 0.0f);

  void addLine(Vec2 from, Vec2 to, Color color, f32 thickness);

  void append(std::span<const DrawCommand> other);

  void appendTranslated(std::span<const DrawCommand> other, Vec2 offset);

private:
  DrawCommand &add(DrawCommandType type);

  [[nodiscard]]
  static constexpr f32 clampRadius(Rect rect, f32 radius) noexcept {
    const f32 smallerSide = rect.w < rect.h ? rect.w : rect.h;
    const f32 maxRadius = smallerSide * 0.5f;

    if (radius > maxRadius) {
      return maxRadius;
    }

    return radius;
  }
  std::vector<DrawCommand> commands;
};
} // namespace octogui