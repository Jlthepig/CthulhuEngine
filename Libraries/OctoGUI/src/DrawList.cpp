#include "OctoGui/DrawList.h"
namespace octogui {

void DrawList::clear() noexcept { commands.clear(); }

void DrawList::reserve(usize count) { commands.reserve(count); }

void DrawList::pushClip(Rect clipRect) {
  DrawCommand &cmd = add(DrawCommandType::PushClip);
  cmd.rect = clipRect;
}

void DrawList::popClip() { add(DrawCommandType::PopClip); }

void DrawList::addRectFilled(Rect rect, Color color) {
  if (rect.isEmpty() || color.isTransparent()) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::Rect);
  cmd.rect = rect;
  cmd.color = color;
  cmd.thickness = 0.0f;
}

void DrawList::addRectOutline(Rect rect, Color color, f32 thickness) {
  if (rect.isEmpty() || color.isTransparent() || thickness <= 0.0f) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::Rect);
  cmd.rect = rect;
  cmd.color = color;
  cmd.thickness = thickness;
}

void DrawList::addImage(TextureHandle texture, Rect rect, Color tint) {
  if (!texture.isValid() || rect.isEmpty() || tint.isTransparent()) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::Rect);
  cmd.rect = rect;
  cmd.color = tint;
  cmd.texture = texture;
  cmd.thickness = 0.0f;
}

void DrawList::addTexturedRect(Rect rect, Rect uv, TextureHandle texture,
                               Color color) {
  if (!texture.isValid() || rect.isEmpty() || uv.isEmpty() ||
      color.isTransparent()) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::TexturedRect);
  cmd.rect = rect;
  cmd.uv = uv;
  cmd.texture = texture;
  cmd.color = color;
  cmd.radius = 0.0f;
  cmd.thickness = 0.0f;
}

void DrawList::addRoundedRectFilled(Rect rect, Color color, f32 radius) {
  if (rect.isEmpty() || color.isTransparent()) {
    return;
  }

  if (radius <= 0.0f) {
    addRectFilled(rect, color);
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::RoundedRect);
  cmd.rect = rect;
  cmd.color = color;
  cmd.radius = clampRadius(rect, radius);
  cmd.thickness = 0.0f;
}

void DrawList::addRoundedRectOutline(Rect rect, Color color, f32 radius,
                                     f32 thickness) {
  if (rect.isEmpty() || color.isTransparent() || thickness <= 0.0f) {
    return;
  }

  if (radius <= 0.0f) {
    addRectOutline(rect, color, thickness);
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::RoundedRect);
  cmd.rect = rect;
  cmd.color = color;
  cmd.radius = clampRadius(rect, radius);
  cmd.thickness = thickness;
}

void DrawList::addGradientRect(Rect rect, const LinearGradient &gradient) {
  if (rect.isEmpty() || !gradient.enabled) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::GradientRect);
  cmd.rect = rect;
  cmd.gradient = gradient;
}

void DrawList::addGradientRoundedRect(Rect rect, const LinearGradient &gradient,
                                      f32 radius) {
  if (rect.isEmpty() || !gradient.enabled) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::GradientRect);
  cmd.rect = rect;
  cmd.radius = radius < 0.0f ? 0.0f : radius;
  cmd.gradient = gradient;
}

void DrawList::addBoxShadow(Rect rect, Color color, f32 radius, Vec2 offset,
                            f32 blur, f32 spread) {
  if (rect.isEmpty() || color.isTransparent()) {
    return;
  }

  if (radius < 0.0f) {
    radius = 0.0f;
  }
  if (blur < 0.0f) {
    blur = 0.0f;
  }

  DrawCommand &cmd = add(DrawCommandType::Shadow);
  cmd.rect = rect;
  cmd.color = color;
  cmd.radius = radius;
  cmd.shadowOffset = offset;
  cmd.shadowBlur = blur;
  cmd.shadowSpread = spread;
}

void DrawList::addLine(Vec2 from, Vec2 to, Color color, f32 thickness) {
  if (color.isTransparent() || thickness <= 0.0f) {
    return;
  }

  DrawCommand &cmd = add(DrawCommandType::Line);
  cmd.start = from;
  cmd.end = to;
  cmd.color = color;
  cmd.thickness = thickness;
}

void DrawList::append(std::span<const DrawCommand> other) {
  commands.insert(commands.end(), other.begin(), other.end());
}

void DrawList::appendTranslated(std::span<const DrawCommand> other,
                                Vec2 offset) {
  if (offset == Vec2::zero()) {
    append(other);
    return;
  }

  commands.reserve(commands.size() + other.size());

  for (const DrawCommand &source : other) {
    DrawCommand command = source;

    switch (command.type) {
    case DrawCommandType::PushClip:
    case DrawCommandType::Rect:
    case DrawCommandType::RoundedRect:
    case DrawCommandType::GradientRect:
    case DrawCommandType::Shadow:
    case DrawCommandType::TexturedRect:
      command.rect = command.rect.translate(offset);
      break;

    case DrawCommandType::Line:
      command.start += offset;
      command.end += offset;
      break;

    case DrawCommandType::Nop:
    case DrawCommandType::PopClip:
      break;
    }

    commands.push_back(command);
  }
}

DrawCommand &DrawList::add(DrawCommandType type) {
  commands.push_back(DrawCommand{});
  DrawCommand &cmd = commands.back();
  cmd.type = type;
  return cmd;
}

} // namespace octogui