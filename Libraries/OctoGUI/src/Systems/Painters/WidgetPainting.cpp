#include "Systems/PaintSystem.h"

#include <string>
#include <charconv>

#include "Internal/LayoutGeometry.h"
#include "Internal/PaintHelpers.h"
#include "Internal/SplitLayoutInternal.h"
#include "Internal/WidgetMetrics.h"
#include "Internal/IconAtlas.h"
#include "Systems/InteractionSystem.h"

namespace octogui {

void PaintSystem::paintCheckbox(const Tree &tree, DrawList &drawList,
                                NodeHandle node,
                                const InteractionState &interaction, Font *font,
                                Rect rect) {
  const VisualStyle *visualStyle = tree.visualStyle(node);
  const StyleState styleState =
      detail::resolveStyleState(tree, node, interaction);

  StateStyle currentState;

  if (visualStyle) {
    currentState = stateStyle(*visualStyle, styleState);
  } else {
    currentState.background = Color::transparent();
    currentState.border = Color::white();
    currentState.text = Color::white();
  }

  const Rect content = detail::contentRect(tree, node, rect);

  if (content.isEmpty()) {
    return;
  }

  f32 boxSize = detail::CheckboxBoxSize;

  if (boxSize > content.h) {
    boxSize = content.h;
  }
  if (boxSize > content.w) {
    boxSize = content.w;
  }
  if (boxSize <= 0.0f) {
    return;
  }

  const f32 boxY = content.y + (content.h - boxSize) * 0.5f;
  const Rect box{content.x, boxY, boxSize, boxSize};

  const f32 cornerRadius = visualStyle ? visualStyle->cornerRadius : 0.0f;
  const f32 borderWidth = visualStyle ? visualStyle->borderWidth : 1.0f;

  const bool checked = hasFlag(tree.flags(node), NodeFlags::Checked);

  Color indicatorColor = currentState.indicator;
  if (indicatorColor.isTransparent()) {
    indicatorColor = currentState.border;
  }
  if (indicatorColor.isTransparent()) {
    indicatorColor = currentState.text;
  }

  const Color outlineColor = checked && !indicatorColor.isTransparent()
                                 ? indicatorColor
                                 : currentState.border;

  if (!currentState.background.isTransparent()) {
    drawList.addRoundedRectFilled(box, currentState.background, cornerRadius);
  }

  if (checked && !indicatorColor.isTransparent()) {
    drawList.addRoundedRectFilled(box, indicatorColor.withAlpha(0.16f),
                                  cornerRadius);
  }

  if (!outlineColor.isTransparent() && borderWidth > 0.0f) {
    drawList.addRoundedRectOutline(box, outlineColor, cornerRadius,
                                   borderWidth);
  }

  if (checked) {
    const Color checkColor = indicatorColor;
    const f32 thickness = 2.0f;

    drawList.addLine(Vec2{box.x + box.w * 0.22f, box.y + box.h * 0.55f},
                     Vec2{box.x + box.w * 0.42f, box.y + box.h * 0.75f},
                     checkColor, thickness);
    drawList.addLine(Vec2{box.x + box.w * 0.42f, box.y + box.h * 0.75f},
                     Vec2{box.x + box.w * 0.78f, box.y + box.h * 0.28f},
                     checkColor, thickness);
  }

  if (visualStyle) {
    detail::addFocusRing(
      drawList,
      box,
      *visualStyle,
      styleState);
  }

  if (!font || !font->isLoaded()) {
    return;
  }

  const std::string_view text = tree.text(node);

  if (text.empty()) {
    return;
  }

  const f32 textX = box.x + box.w + detail::CheckboxSpacing;
  const f32 textWidth = (content.x + content.w) - textX;

  if (textWidth <= 0.0f || content.h <= 0.0f) {
    return;
  }

  const Rect textRect{textX, content.y, textWidth, content.h};

  const f32 textHeight =
      detail::fontTextHeight(font->metrics());
  const f32 textTop =
      detail::centeredTextTop(textRect, textHeight);

  drawList.pushClip(textRect);
  font->drawText(drawList, text, Vec2{textX, textTop}, currentState.text);
  drawList.popClip();
}

void PaintSystem::paintSlider(const Tree &tree, DrawList &drawList,
                              NodeHandle node,
                              const InteractionState &interaction, Rect rect) {
  const SliderState *sliderState = tree.sliderState(node);

  if (!sliderState) {
    return;
  }

  const VisualStyle *visualStyle = tree.visualStyle(node);
  const StyleState styleState =
      detail::resolveStyleState(tree, node, interaction);

  StateStyle currentState;

  if (visualStyle) {
    currentState = stateStyle(*visualStyle, styleState);
  } else {
    currentState.background = Color::white();
    currentState.border = Color::white();
    currentState.text = Color::white();
  }

  const Rect content = detail::contentRect(tree, node, rect);

  if (content.isEmpty()) {
    return;
  }

  const f32 baseHandleRadius = detail::sliderHandleRadiusForContent(content);

  f32 drawHandleRadius = baseHandleRadius;

  if (styleState == StyleState::Hovered) {
    drawHandleRadius += 0.5f;
  }
  if (styleState == StyleState::Active) {
    drawHandleRadius += 1.0f;
  }

  const f32 maxHandleRadius = content.h * 0.5f;

  if (drawHandleRadius > maxHandleRadius) {
    drawHandleRadius = maxHandleRadius;
  }
  if (drawHandleRadius < 2.0f) {
    drawHandleRadius = 2.0f;
  }

  f32 trackHeight = detail::SliderTrackHeight;

  if (trackHeight > content.h) {
    trackHeight = content.h;
  }
  if (trackHeight <= 0.0f) {
    return;
  }

  const Rect track{content.x, content.y + (content.h - trackHeight) * 0.5f,
                   content.w, trackHeight};

  f32 range = sliderState->maxValue - sliderState->minValue;

  if (range < 0.0f) {
    range = 0.0f;
  }

  f32 t = 0.0f;

  if (range > 0.0f) {
    t = (sliderState->value - sliderState->minValue) / range;
  }

  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }

  f32 usableWidth = track.w - baseHandleRadius * 2.0f;

  if (usableWidth < 0.0f) {
    usableWidth = 0.0f;
  }

  const f32 handleX = track.x + baseHandleRadius + usableWidth * t;
  const f32 trackRadius = trackHeight * 0.5f;

  Color trackColor = currentState.background;

  if (trackColor.isTransparent()) {
    trackColor = Color::white().withAlpha(0.15f);
  }

  Color fillColor = currentState.indicator;

  if (fillColor.isTransparent()) {
    fillColor = currentState.text;
  }

  if (fillColor.isTransparent()) {
    fillColor = currentState.border;
  }
  if (fillColor.isTransparent()) {
    fillColor = Color::white();
  }

  Color handleColor = currentState.border;

  if (handleColor.isTransparent()) {
    handleColor = fillColor;
  }
  if (handleColor.isTransparent()) {
    handleColor = Color::white();
  }

  const Color handleOutlineColor = Color::black().withAlpha(0.35f);

  if (!trackColor.isTransparent()) {
    drawList.addRoundedRectFilled(track, trackColor, trackRadius);
  }

  const f32 fillWidth = handleX - track.x;

  if (fillWidth > 0.0f && !fillColor.isTransparent()) {
    const Rect fill{track.x, track.y, fillWidth, trackHeight};
    drawList.addRoundedRectFilled(fill, fillColor, trackRadius);
  }

  const Rect handleRect{handleX - drawHandleRadius,
                        track.y + trackHeight * 0.5f - drawHandleRadius,
                        drawHandleRadius * 2.0f, drawHandleRadius * 2.0f};

  drawList.addRoundedRectFilled(handleRect, handleColor, drawHandleRadius);
  drawList.addRoundedRectOutline(handleRect, handleOutlineColor,
                                 drawHandleRadius, 1.0f);
}

void PaintSystem::paintProgressBar(
    const Tree& tree,
    DrawList& drawList,
    NodeHandle node,
    Font* font,
    Rect rect) {
  const ProgressBarState* progress =
      tree.progressBarState(node);

  if (!progress ||
      rect.isEmpty()) {
    return;
  }

  const VisualStyle* visualStyle =
      tree.visualStyle(node);

  StateStyle currentState;

  if (visualStyle) {
    currentState =
        stateStyle(
            *visualStyle,
            StyleState::Normal);
  } else {
    currentState.background =
        Color::white().withAlpha(0.15f);

    currentState.border =
        Color::white().withAlpha(0.25f);

    currentState.indicator =
        Color::white();

    currentState.text =
        Color::white();
  }

  const f32 radius =
      visualStyle
          ? visualStyle->cornerRadius
          : rect.h * 0.25f;

  const f32 borderWidth =
      visualStyle
          ? visualStyle->borderWidth
          : 1.0f;

  Color trackColor =
      currentState.background;

  if (trackColor.isTransparent()) {
    trackColor =
        Color::white().withAlpha(0.12f);
  }

  Color fillColor =
      currentState.indicator;

  if (fillColor.isTransparent()) {
    fillColor =
        currentState.border;
  }

  if (fillColor.isTransparent()) {
    fillColor =
        Color::white();
  }

  drawList.addRoundedRectFilled(
      rect,
      trackColor,
      radius);

  f64 range =
      progress->maxValue -
      progress->minValue;

  f64 normalized = 0.0;

  if (range > 0.0) {
    normalized =
        (progress->value -
         progress->minValue) /
        range;
  }

  if (normalized < 0.0) {
    normalized = 0.0;
  }

  if (normalized > 1.0) {
    normalized = 1.0;
  }

  const f32 inset =
      detail::ProgressBarFillInset;

  Rect fillArea{
      rect.x + inset,
      rect.y + inset,
      rect.w - inset * 2.0f,
      rect.h - inset * 2.0f};

  if (fillArea.w < 0.0f) {
    fillArea.w = 0.0f;
  }

  if (fillArea.h < 0.0f) {
    fillArea.h = 0.0f;
  }

  const f32 fillWidth =
      fillArea.w *
      static_cast<f32>(normalized);

  if (fillWidth > 0.0f &&
      fillArea.h > 0.0f) {
    const Rect fill{
        fillArea.x,
        fillArea.y,
        fillWidth,
        fillArea.h};

    f32 fillRadius =
        radius - inset;

    if (fillRadius < 0.0f) {
      fillRadius = 0.0f;
    }

    drawList.addRoundedRectFilled(
        fill,
        fillColor,
        fillRadius);
  }

  if (borderWidth > 0.0f &&
      !currentState.border.isTransparent()) {
    drawList.addRoundedRectOutline(
        rect,
        currentState.border,
        radius,
        borderWidth);
  }

  if (!progress->showPercentage ||
      !font ||
      !font->isLoaded() ||
      currentState.text.isTransparent()) {
    return;
  }

  const i32 percentage =
      static_cast<i32>(
          normalized * 100.0 + 0.5);
    char buffer[8]{};

  auto result =
      std::to_chars(
          buffer,
          buffer + sizeof(buffer) - 1,
          percentage);

  if (result.ec != std::errc{}) {
    return;
  }

  *result.ptr++ = '%';

  const std::string_view text{
      buffer,
      static_cast<usize>(
          result.ptr - buffer)};

  const Vec2 textSize =
      font->measureText(text);

  const f32 x =
      rect.x +
      (rect.w - textSize.x) *
          0.5f;

    const f32 textHeight =
      detail::fontTextHeight(
        font->metrics());

  const f32 y =
      rect.y +
      (rect.h - textHeight) *
          0.5f;

  drawList.pushClip(rect);

  font->drawText(
      drawList,
      text,
      Vec2{x, y},
      currentState.text);

  drawList.popClip();
}

void PaintSystem::paintRadioButton(const Tree &tree, DrawList &drawList,
                                   NodeHandle node,
                                   const InteractionState &interaction,
                                   Font *font, Rect rect) {
  const VisualStyle *visualStyle = tree.visualStyle(node);
  const StyleState styleState =
      detail::resolveStyleState(tree, node, interaction);

  StateStyle currentState;

  if (visualStyle) {
    currentState = stateStyle(*visualStyle, styleState);
  } else {
    currentState.background = Color::transparent();
    currentState.border = Color::white();
    currentState.text = Color::white();
  }

  const Rect content = detail::contentRect(tree, node, rect);

  if (content.isEmpty()) {
    return;
  }

  f32 boxSize = detail::RadioButtonBoxSize;

  if (boxSize > content.h) {
    boxSize = content.h;
  }
  if (boxSize > content.w) {
    boxSize = content.w;
  }
  if (boxSize <= 0.0f) {
    return;
  }

  const f32 boxY = content.y + (content.h - boxSize) * 0.5f;
  const Rect box{content.x, boxY, boxSize, boxSize};

  const f32 outerRadius = boxSize * 0.5f;
  const f32 borderWidth = visualStyle ? visualStyle->borderWidth : 1.0f;

  const bool checked = hasFlag(tree.flags(node), NodeFlags::Checked);

  Color indicatorColor = currentState.indicator;
  if (indicatorColor.isTransparent()) {
    indicatorColor = currentState.border;
  }
  if (indicatorColor.isTransparent()) {
    indicatorColor = currentState.text;
  }

  const Color outlineColor = checked && !indicatorColor.isTransparent()
                                 ? indicatorColor
                                 : currentState.border;

  if (!currentState.background.isTransparent()) {
    drawList.addRoundedRectFilled(box, currentState.background, outerRadius);
  }
  if (!currentState.border.isTransparent() && borderWidth > 0.0f) {
    drawList.addRoundedRectOutline(box, outlineColor, outerRadius, borderWidth);
  }

  if (checked) {
    Color dotColor = indicatorColor;

    if (dotColor.isTransparent()) {
      dotColor = currentState.border;
    }
    if (dotColor.isTransparent()) {
      dotColor = Color::white();
    }

    const f32 centerX = box.x + box.w * 0.5f;
    const f32 centerY = box.y + box.h * 0.5f;
    const f32 dotRadius = boxSize * 0.28f;

    if (dotRadius > 0.0f) {
      const Rect dot{centerX - dotRadius, centerY - dotRadius, dotRadius * 2.0f,
                     dotRadius * 2.0f};
      drawList.addRoundedRectFilled(dot, dotColor, dotRadius);
    }
  }

  if (visualStyle) {
    detail::addFocusRing(
      drawList,
      box,
      *visualStyle,
      styleState);
  }

  if (!font || !font->isLoaded()) {
    return;
  }

  const std::string_view text = tree.text(node);

  if (text.empty()) {
    return;
  }

  const f32 textX = box.x + box.w + detail::RadioButtonSpacing;
  const f32 textWidth = (content.x + content.w) - textX;

  if (textWidth <= 0.0f || content.h <= 0.0f) {
    return;
  }

  const Rect textRect{textX, content.y, textWidth, content.h};

  const f32 textHeight =
      detail::fontTextHeight(font->metrics());
  const f32 textTop =
      detail::centeredTextTop(textRect, textHeight);

  drawList.pushClip(textRect);
  font->drawText(drawList, text, Vec2{textX, textTop}, currentState.text);
  drawList.popClip();
}

void PaintSystem::paintSeparator(const Tree& tree, DrawList& drawList,
                                 NodeHandle node,
                                 const InteractionState& interaction,
                                 Rect rect) {
  const SeparatorState* separator = tree.separatorState(node);
  if (!separator) {return;}

  const Rect content = detail::contentRect(tree, node, rect);
  if (content.isEmpty()) {return;}

  const VisualStyle* visualStyle = tree.visualStyle(node);

  Color color = Color::white().withAlpha(0.25f);
  f32 thickness = 1.0f;

  if (visualStyle) {
    const StyleState styleState =
      detail::resolveStyleState(tree, node, interaction);
    const StateStyle& currentState = stateStyle(*visualStyle, styleState);

    if (!currentState.indicator.isTransparent()) {
      color = currentState.indicator;
    }

    if (visualStyle->borderWidth > 0.0f) {
      thickness = visualStyle->borderWidth;
    }
  }

  if (separator->orientation == SeparatorOrientation::Horizontal) {
    const f32 y = content.y + (content.h - thickness) * 0.5f;

    drawList.addRectFilled(
        Rect{content.x, y, content.w, thickness},
        color
    );

    return;
  }

  const f32 x = content.x + (content.w - thickness) * 0.5f;

  drawList.addRectFilled(
      Rect{x, content.y, thickness, content.h},
      color
  );
}

void PaintSystem::paintIcon(
    const Tree& tree,
    DrawList& drawList,
    NodeHandle node,
    const IconAtlas& icons,
    Rect rect) {
  const IconState* icon =
      tree.iconState(node);

  if (!icon ||
      icon->name.empty() ||
      icon->color.isTransparent() ||
      rect.isEmpty()) {
    return;
  }

  const Rect content =
    detail::contentRect(
        tree,
        node,
        rect);

  if (content.isEmpty()) {
    return;
  }

  ResolvedIcon resolved{};

  if (!icons.lookup(
          icon->name,
          resolved)) {
    return;
  }

  const f32 size =
    content.w < content.h
        ? content.w
        : content.h;

  if (size <= 0.0f) {
    return;
  }

  const Rect iconRect{
    content.x +
        (content.w - size) * 0.5f,
    content.y +
        (content.h - size) * 0.5f,
    size,
    size};

  drawList.addTexturedRect(
      iconRect,
      resolved.uv,
      resolved.texture,
      icon->color);
}

void PaintSystem::paintNodeText(const Tree& tree, DrawList& drawList,
                                Font& font, NodeHandle node, Rect nodeRect,
                                Color color,
                                const InteractionState& interaction) {
  const TextState* textState =
      tree.textState(node);

  if (!textState) {return;}

  const bool isTextInput =
      tree.type(node) == NodeType::TextInput || tree.type(node) == NodeType::NumericInput;

  const TextInputState* inputState =
      isTextInput
          ? tree.textInputState(node)
          : nullptr;

  const bool focused =
      isTextInput &&
      interaction.focused == node;

  bool showingPlaceholder = false;

  std::string_view displayText =
      textState->text;

  if (inputState &&
      displayText.empty() &&
      !inputState->placeholder.empty()) {
    displayText =
        inputState->placeholder;

    showingPlaceholder = true;
  }

  if (!isTextInput &&
      (displayText.empty() ||
       color.isTransparent())) {
    return;
  }

  const Rect content =
      detail::contentRect(tree, node, nodeRect);

  if (content.isEmpty()) {return;}

  const FontMetrics metrics =
      font.metrics();

  const Vec2 textSize =
      displayText.empty()
          ? Vec2{0.0f, metrics.lineHeight}
          : font.measureText(displayText);

  const f32 textWidth =
      textSize.x;

    const f32 textHeight =
      detail::fontTextHeight(metrics);

  if (textHeight <= 0.0f) {return;}

  f32 x = content.x;

  if (!isTextInput &&
      content.w >= textWidth) {
    if (textState->horizontalAlign ==
        TextAlign::Center) {
      x +=
          (content.w - textWidth) * 0.5f;
    } else if (textState->horizontalAlign ==
               TextAlign::End) {
      x +=
          content.w - textWidth;
    }
  }

  f32 top = content.y;

  if (content.h >= textHeight) {
    if (textState->verticalAlign ==
        TextAlign::Center) {
      top +=
          (content.h - textHeight) * 0.5f;
    } else if (textState->verticalAlign ==
               TextAlign::End) {
      top +=
          content.h - textHeight;
    }
  }

  f32 inputTextX = content.x;

  if (inputState) {
    inputTextX -=
        inputState->scrollX;

    if (!showingPlaceholder) {
      x = inputTextX;
    }
  }

  Color indicatorColor =
      color;

  if (isTextInput) {
    if (const VisualStyle* visualStyle =
            tree.visualStyle(node)) {
      const StyleState styleState =
          detail::resolveStyleState(
              tree,
              node,
              interaction
          );

      const StateStyle& currentState =
          stateStyle(
              *visualStyle,
              styleState
          );

      indicatorColor =
          currentState.indicator;

      if (indicatorColor.isTransparent()) {
        indicatorColor =
            currentState.border;
      }
    }

    if (indicatorColor.isTransparent()) {
      indicatorColor = color;
    }

    if (indicatorColor.isTransparent()) {
      indicatorColor =
          Color::white();
    }
  }

  drawList.pushClip(content);

  if (focused &&
      inputState &&
      inputState->cursor !=
          inputState->selectionAnchor &&
      !textState->text.empty()) {
    const u32 selectionStart =
        inputState->cursor <
                inputState->selectionAnchor
            ? inputState->cursor
            : inputState->selectionAnchor;

    const u32 selectionEnd =
        inputState->cursor >
                inputState->selectionAnchor
            ? inputState->cursor
            : inputState->selectionAnchor;

    f32 selectionX1 =
        inputTextX +
        font.caretX(
            textState->text,
            selectionStart
        );

    f32 selectionX2 =
        inputTextX +
        font.caretX(
            textState->text,
            selectionEnd
        );

    if (selectionX2 < selectionX1) {
      const f32 temporary =
          selectionX1;

      selectionX1 =
          selectionX2;

      selectionX2 =
          temporary;
    }

    const f32 selectionWidth =
        selectionX2 - selectionX1;

    if (selectionWidth > 0.0f) {
      const Color selectionColor =
          indicatorColor.withAlpha(
              indicatorColor.a *
              detail::TextInputSelectionAlpha
          );

      drawList.addRectFilled(
          Rect{
              selectionX1,
              top,
              selectionWidth,
              textHeight
          },
          selectionColor
      );
    }
  }

  if (!displayText.empty()) {
    Color drawColor = color;

    if (showingPlaceholder) {
      drawColor =
          color.withAlpha(
              color.a *
              detail::TextInputPlaceholderAlpha
          );

      x = content.x;
    }

    if (!drawColor.isTransparent()) {
      if (isTextInput) {
        font.drawTextClipped(
            drawList,
            displayText,
            Vec2{x, top},
            drawColor,
            content
        );
      } else {
        font.drawText(
            drawList,
            displayText,
            Vec2{x, top},
            drawColor
        );
      }
    }
  }

  if (focused &&
      inputState &&
      inputState->caretBlinkTimer <
          detail::TextInputCaretVisibleDuration) {
    const f32 caretX =
        inputTextX +
        font.caretX(
            textState->text,
            inputState->cursor
        );

    drawList.addRectFilled(
        Rect{
            caretX,
            top,
            detail::TextInputCaretWidth,
            textHeight
        },
        indicatorColor
    );
  }

  drawList.popClip();
}

void PaintSystem::paintCollapsibleSectionIndicator(
    const Tree& tree,
    DrawList& drawList,
    NodeHandle node,
    Rect rect,
    Color color) {
  const CollapsibleSectionState* state =
      tree.collapsibleSectionState(node);

  if (!state ||
      color.isTransparent()) {
    return;
  }

  const bool collapsed =
      !tree.isValid(state->content) ||
      !tree.isVisible(state->content);

  const f32 size =
      detail::CollapsibleSectionChevronSize;

  const f32 half =
      size * 0.5f;

  const f32 centerX =
      rect.x +
      detail::CollapsibleSectionChevronInset +
      half;

  const f32 centerY =
      rect.y +
      rect.h * 0.5f;

  if (collapsed) {
    const Vec2 top{
        centerX - half * 0.35f,
        centerY - half};

    const Vec2 middle{
        centerX + half * 0.35f,
        centerY};

    const Vec2 bottom{
        centerX - half * 0.35f,
        centerY + half};

    drawList.addLine(
        top,
        middle,
        color,
        detail::CollapsibleSectionChevronThickness);

    drawList.addLine(
        middle,
        bottom,
        color,
        detail::CollapsibleSectionChevronThickness);

    return;
  }

  const Vec2 left{
      centerX - half,
      centerY - half * 0.35f};

  const Vec2 middle{
      centerX,
      centerY + half * 0.35f};

  const Vec2 right{
      centerX + half,
      centerY - half * 0.35f};

  drawList.addLine(
      left,
      middle,
      color,
      detail::CollapsibleSectionChevronThickness);

  drawList.addLine(
      middle,
      right,
      color,
      detail::CollapsibleSectionChevronThickness);
}

void PaintSystem::paintSplitContainerDivider(
    const Tree& tree,
    DrawList& drawList,
    NodeHandle node,
    const InteractionState& interaction,
    Rect rect) {
  const SplitContainerState* split = tree.splitContainerState(node);

  if (!split) {
    return;
  }

  const detail::SplitGeometry geometry =
      detail::splitGeometry(tree, node, rect);

  if (geometry.divider.isEmpty()) {
    return;
  }

  Color color = Color::white().withAlpha(0.22f);

  const VisualStyle* style = tree.visualStyle(node);

  if (style) {
    const StateStyle& normal = stateStyle(*style, StyleState::Normal);

    if (!normal.indicator.isTransparent()) {
      color = normal.indicator;
    }
  }

  if (interaction.splitterActive == node) {
    color = color.withAlpha(1.0f);
  } else if (interaction.splitterHovered == node) {
    color = color.withAlpha(0.75f);
  } else {
    color = color.withAlpha(0.45f);
  }

  drawList.addRectFilled(geometry.divider, color);
}

} // namespace octogui
