#include <charconv>
#include <cmath>
#include <string>

#include "Systems/EventSystem.h"
#include "Systems/StyleSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/LayoutGeometry.h"
#include "Internal/WidgetMetrics.h"

namespace octogui {
namespace {

// UTF-8 helpers

bool isUtf8Continuation(char value) noexcept {
  const u8 byte = static_cast<u8>(value);
  return (byte & 0xC0u) == 0x80u;
}

u32 clampUtf8Offset(std::string_view text, u32 offset) noexcept {
  const u32 size = static_cast<u32>(text.size());

  if (offset > size) {
    offset = size;
  }

  while (offset > 0 && offset < size && isUtf8Continuation(text[offset])) {
    --offset;
  }

  return offset;
}

u32 previousUtf8Offset(std::string_view text, u32 offset) noexcept {
  offset = clampUtf8Offset(text, offset);

  if (offset == 0) {
    return 0;
  }

  --offset;

  while (offset > 0 && isUtf8Continuation(text[offset])) {
    --offset;
  }

  return offset;
}

u32 nextUtf8Offset(std::string_view text, u32 offset) noexcept {
  const u32 size = static_cast<u32>(text.size());

  offset = clampUtf8Offset(text, offset);

  if (offset >= size) {
    return size;
  }

  ++offset;

  while (offset < size && isUtf8Continuation(text[offset])) {
    ++offset;
  }

  return offset;
}

u32 utf8CharacterCount(std::string_view text) noexcept {
  u32 count = 0;

  for (char character : text) {
    if (!isUtf8Continuation(static_cast<u8>(character))) {
      ++count;
    }
  }

  return count;
}

usize utf8PrefixBytes(std::string_view text, u32 maxCharacters) noexcept {
  if (maxCharacters == 0) {
    return 0;
  }

  u32 count = 0;
  usize offset = 0;

  while (offset < text.size()) {
    if (!isUtf8Continuation(static_cast<u8>(text[offset]))) {
      if (count == maxCharacters) {
        break;
      }

      ++count;
    }

    ++offset;
  }

  return offset;
}

bool validTextCodepoint(u32 codepoint) noexcept {
  if (codepoint > 0x10FFFFu) {
    return false;
  }
  if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) {
    return false;
  }

  if (codepoint <= 0x1Fu || codepoint == 0x7Fu) {
    return false;
  }

  return true;
}

u32 encodeUtf8(u32 codepoint, char output[4]) noexcept {
  if (!validTextCodepoint(codepoint)) {
    return 0;
  }

  if (codepoint <= 0x7Fu) {
    output[0] = static_cast<char>(codepoint);
    return 1;
  }

  if (codepoint <= 0x7FFu) {
    output[0] = static_cast<char>(0xC0u | (codepoint >> 6u));
    output[1] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
    return 2;
  }

  if (codepoint <= 0xFFFFu) {
    output[0] = static_cast<char>(0xE0u | (codepoint >> 12u));
    output[1] = static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu));
    output[2] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
    return 3;
  }

  output[0] = static_cast<char>(0xF0u | (codepoint >> 18u));
  output[1] = static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu));
  output[2] = static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu));
  output[3] = static_cast<char>(0x80u | (codepoint & 0x3Fu));

  return 4;
}

// Text selection / cursor helpers

bool hasSelection(const TextInputState &state) noexcept {
  return state.cursor != state.selectionAnchor;
}

u32 selectionStart(const TextInputState &state) noexcept {
  return state.cursor < state.selectionAnchor ? state.cursor
                                              : state.selectionAnchor;
}

u32 selectionEnd(const TextInputState &state) noexcept {
  return state.cursor > state.selectionAnchor ? state.cursor
                                              : state.selectionAnchor;
}

void normalizeTextInputState(TextInputState &state,
                             std::string_view text) noexcept {
  state.cursor = clampUtf8Offset(text, state.cursor);
  state.selectionAnchor = clampUtf8Offset(text, state.selectionAnchor);
}

bool deleteSelection(TextState &textState, TextInputState &inputState) {
  if (!hasSelection(inputState)) {
    return false;
  }

  const u32 start = selectionStart(inputState);
  const u32 end = selectionEnd(inputState);

  textState.text.erase(start, end - start);

  inputState.cursor = start;
  inputState.selectionAnchor = start;

  return true;
}

void moveCursor(TextInputState &state, u32 position, bool selecting) noexcept {
  state.cursor = position;

  if (!selecting) {
    state.selectionAnchor = position;
  }
}

void touchTextInput(Tree &tree, NodeHandle handle, TextInputState &state) {
  state.caretBlinkTimer = 0.0f;
  tree.markPaintDirty(handle);
}

// Clipboard helpers

std::string sanitizeSingleLineClipboard(std::string_view text, u32 maxCharacters) {
  std::string result;

  const usize reserveBytes =
      text.size() < static_cast<usize>(maxCharacters) * 4
          ? text.size()
          : static_cast<usize>(maxCharacters) * 4;

  result.reserve(reserveBytes);

  u32 characters = 0;

  for (usize i = 0; i < text.size();) {
    const u8 byte =
        static_cast<u8>(text[i]);

    if (byte < 0x20u || byte == 0x7Fu) {
      ++i;
      continue;
    }

    if (characters >= maxCharacters) {
      break;
    }

    const usize start = i;

    ++i;

    while (i < text.size() &&
           isUtf8Continuation(
               static_cast<u8>(text[i]))) {
      ++i;
    }

    result.append(
        text.data() + start,
        i - start);

    ++characters;
  }

  return result;
}

// Numeric input helpers

bool isValidIntegerEdit(std::string_view text) noexcept {
  if (text.empty()) {
    return true;
  }

  usize index = 0;

  if (text[0] == '-') {
    index = 1;
  }

  for (; index < text.size(); ++index) {
    if (text[index] < '0' || text[index] > '9') {
      return false;
    }
  }

  return true;
}

bool isValidFloatEdit(std::string_view text) noexcept {
  if (text.empty()) {
    return true;
  }

  bool hasDigit = false;
  bool hasDecimal = false;
  bool hasExponent = false;

  for (usize i = 0; i < text.size(); ++i) {
    const char character = text[i];

    if (character >= '0' && character <= '9') {
      hasDigit = true;
      continue;
    }

    if (character == '-') {
      if (i == 0) {
        continue;
      }

      if (i > 0 && (text[i - 1] == 'e' || text[i - 1] == 'E')) {
        continue;
      }

      return false;
    }

    if (character == '+') {
      if (i > 0 && (text[i - 1] == 'e' || text[i - 1] == 'E')) {
        continue;
      }

      return false;
    }

    if (character == '.') {
      if (hasDecimal || hasExponent) {
        return false;
      }

      hasDecimal = true;
      continue;
    }

    if (character == 'e' || character == 'E') {
      if (hasExponent || !hasDigit) {
        return false;
      }

      hasExponent = true;
      continue;
    }

    return false;
  }

  return true;
}

bool isValidNumericEdit(std::string_view text, NumericInputType type) noexcept {
  if (type == NumericInputType::Integer) {
    return isValidIntegerEdit(text);
  }

  return isValidFloatEdit(text);
}

bool parseNumericText(std::string_view text, f64& value) noexcept {
  if (text.empty()) {
    return false;
  }

  const char* begin = text.data();
  const char* end = begin + text.size();

  f64 parsed = 0.0;

  const auto result =
      std::from_chars(begin, end, parsed, std::chars_format::general);

  if (result.ec != std::errc{} || result.ptr != end) {
    return false;
  }

  if (!std::isfinite(parsed)) {
    return false;
  }

  value = parsed;
  return true;
}

std::string formatNumericValue(f64 value, NumericInputType type) {
  char buffer[128];

  std::to_chars_result result;

  if (type == NumericInputType::Integer) {
    value = std::round(value);

    result = std::to_chars(
        buffer,
        buffer + sizeof(buffer),
        value,
        std::chars_format::fixed,
        0);
  } else {
    result = std::to_chars(
        buffer,
        buffer + sizeof(buffer),
        value,
        std::chars_format::general,
        12);
  }

  if (result.ec != std::errc{}) {
    return "0";
  }

  return std::string(buffer, result.ptr);
}

} // namespace

f64 WidgetSystem::normalizeNumericValue(const NumericInputState& state, f64 value) const noexcept {
  if (!std::isfinite(value)) {
    return state.value;
  }

  if (state.type == NumericInputType::Integer) {
    value = std::round(value);
  }

  if (value < state.minValue) {
    value = state.minValue;
  }

  if (value > state.maxValue) {
    value = state.maxValue;
  }

  if (state.type == NumericInputType::Integer) {
    value = std::round(value);

    if (value < state.minValue) {
      value = std::ceil(state.minValue);
    }

    if (value > state.maxValue) {
      value = std::floor(state.maxValue);
    }
  }

  return value;
}

// TextInput

NodeHandle WidgetSystem::createTextInput(Tree &tree, StyleSystem &styles,
                                         NodeHandle parent,
                                         std::string_view text,
                                         std::string_view placeholder) {
  NodeHandle handle = createNode(tree, styles, NodeType::TextInput, parent);

  tree.setTextAlign(handle, TextAlign::Start, TextAlign::Center);

  LayoutStyle *layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize = Vec2{180.0f, 34.0f};
    layout->minSize = Vec2{60.0f, 28.0f};
    layout->padding = Padding{10.0f, 6.0f};
  }

  TextState* textState = tree.textState(handle);

  if (textState) {
    textState->text.reserve(64);
  }

  if (!text.empty()) {
    tree.setText(handle, text);
  }

  TextInputState *state = tree.textInputState(handle);

  if (state) {
    const u32 textSize = static_cast<u32>(tree.text(handle).size());

    state->cursor = textSize;
    state->selectionAnchor = textSize;
    state->placeholder.assign(placeholder);
  }

  return handle;
}

void WidgetSystem::setTextInputPlaceholder(Tree &tree, NodeHandle handle,
                                           std::string_view placeholder) {
  TextInputState *state = tree.textInputState(handle);

  if (!state || state->placeholder == placeholder) {
    return;
  }

  state->placeholder.assign(placeholder);
  tree.markPaintDirty(handle);
}

std::string_view
WidgetSystem::textInputPlaceholder(const Tree &tree,
                                   NodeHandle handle) const noexcept {
  const TextInputState *state = tree.textInputState(handle);

  if (!state) {
    return {};
  }

  return state->placeholder;
}

void WidgetSystem::setTextInputReadOnly(Tree &tree, NodeHandle handle,
                                        bool readOnly) {
  TextInputState *state = tree.textInputState(handle);

  if (!state || state->readOnly == readOnly) {
    return;
  }

  state->readOnly = readOnly;
  tree.markPaintDirty(handle);
}

bool WidgetSystem::isTextInputReadOnly(const Tree &tree,
                                       NodeHandle handle) const noexcept {
  const TextInputState *state = tree.textInputState(handle);
  return state && state->readOnly;
}

f32 WidgetSystem::maxTextInputScrollX(const Tree &tree, Font &font,
                                      NodeHandle handle) const {
  const TextState *textState = tree.textState(handle);

  if (!textState) {
    return 0.0f;
  }

  const Rect content = detail::contentRect(tree, handle, tree.rect(handle));

  if (content.w <= 0.0f) {
    return 0.0f;
  }

  const f32 textWidth = font.measureText(textState->text).x;

  f32 maxScroll = textWidth - content.w + detail::TextInputCaretScrollMargin;

  if (maxScroll < 0.0f) {
    maxScroll = 0.0f;
  }

  return maxScroll;
}

void WidgetSystem::ensureTextInputCaretVisible(Tree &tree, Font &font,
                                               NodeHandle handle) {
  TextInputState *inputState = tree.textInputState(handle);
  const TextState *textState = tree.textState(handle);

  if (!inputState || !textState) {
    return;
  }

  const Rect content = detail::contentRect(tree, handle, tree.rect(handle));

  if (content.w <= 0.0f) {
    inputState->scrollX = 0.0f;
    return;
  }

  const f32 maxScroll = maxTextInputScrollX(tree, font, handle);

  if (maxScroll <= 0.0f) {
    if (inputState->scrollX != 0.0f) {
      inputState->scrollX = 0.0f;
      tree.markPaintDirty(handle);
    }

    return;
  }

  const f32 caretX = font.caretX(textState->text, inputState->cursor);

  const f32 margin = detail::TextInputCaretScrollMargin;

  const f32 visibleLeft = inputState->scrollX + margin;

  const f32 visibleRight = inputState->scrollX + content.w - margin;

  f32 newScroll = inputState->scrollX;

  if (caretX < visibleLeft) {
    newScroll = caretX - margin;
  } else if (caretX > visibleRight) {
    newScroll = caretX - content.w + margin;
  }

  if (newScroll < 0.0f) {
    newScroll = 0.0f;
  }

  if (newScroll > maxScroll) {
    newScroll = maxScroll;
  }

  if (newScroll == inputState->scrollX) {
    return;
  }

  inputState->scrollX = newScroll;
  tree.markPaintDirty(handle);
}

void WidgetSystem::handleTextInputEvent(Tree &tree, Font &font,
                                        NodeHandle handle,
                                        const InputEvent &event, const ClipboardCallbacks& clipboard) {
  if (!tree.isValid(handle)) {
    return;
  }

  const NodeType type = tree.type(handle);

  if (type != NodeType::TextInput && type != NodeType::NumericInput) {
    return;
  }
                                          
  TextInputState *inputState = tree.textInputState(handle);
  TextState *textState = tree.textState(handle);

  if (!inputState || !textState) {
    return;
  }

  normalizeTextInputState(*inputState, textState->text);

  if (event.type == InputEventType::Text) {
    if (inputState->readOnly) {
      return;
    }

    char encoded[4]{};
    const u32 encodedSize = encodeUtf8(event.codepoint, encoded);

    if (encodedSize == 0) {
      return;
    }

    deleteSelection(*textState, *inputState);

    const u32 currentCharacters =
    utf8CharacterCount(textState->text);

    const u32 selectedCharacters =
        hasSelection(*inputState)
            ? utf8CharacterCount(
                  std::string_view{
                      textState->text.data() +
                          selectionStart(*inputState),
                      selectionEnd(*inputState) -
                          selectionStart(*inputState)})
            : 0;

    const u32 charactersAfterSelection =
    currentCharacters - selectedCharacters;

    if (charactersAfterSelection >=
        inputState->maxCharacters) {
      return;
    }

    textState->text.insert(inputState->cursor, encoded, encodedSize);

    inputState->cursor += encodedSize;
    inputState->selectionAnchor = inputState->cursor;

    touchTextInput(tree, handle, *inputState);
    ensureTextInputCaretVisible(tree, font, handle);
    return;
  }

  if (event.type != InputEventType::Key) {
    return;
  }

  if (event.action == KeyAction::Release) {
    return;
  }

  const bool selecting = event.modifiers.shift;

  // Ctrl on Windows/Linux and Command on macOS
  const bool shortcut = event.modifiers.ctrl || event.modifiers.super;

    if (shortcut &&
      event.action == KeyAction::Press &&
      event.key == Key::C) {
    if (!hasSelection(*inputState) ||
        !clipboard.setText) {
      return;
    }

    const u32 start =
        selectionStart(*inputState);

    const u32 end =
        selectionEnd(*inputState);

    const std::string_view selected{
        textState->text.data() + start,
        end - start
    };

    clipboard.setText(
        clipboard.userData,
        selected
    );

    return;
  }

  if (shortcut &&
    event.action == KeyAction::Press &&
    event.key == Key::X) {
  if (inputState->readOnly ||
      !hasSelection(*inputState) ||
      !clipboard.setText) {
    return;
  }

  const u32 start =
      selectionStart(*inputState);

  const u32 end =
      selectionEnd(*inputState);

  const std::string_view selected{
      textState->text.data() + start,
      end - start
  };

  clipboard.setText(
      clipboard.userData,
      selected
  );

  deleteSelection(
      *textState,
      *inputState
  );

  touchTextInput(
      tree,
      handle,
      *inputState
  );

  ensureTextInputCaretVisible(
      tree,
      font,
      handle
  );

  return;
}

  if (shortcut &&
    event.action == KeyAction::Press &&
    event.key == Key::V) {
    if (inputState->readOnly ||
        !clipboard.getText) {
      return;
    }

    const u32 currentCharacters =
    utf8CharacterCount(textState->text);

    const u32 selectedCharacters =
        hasSelection(*inputState)
            ? utf8CharacterCount(
                  std::string_view{
                      textState->text.data() +
                          selectionStart(*inputState),
                      selectionEnd(*inputState) -
                          selectionStart(*inputState)})
            : 0;

    const u32 usedCharacters =
        currentCharacters - selectedCharacters;

    const u32 availableCharacters =
        usedCharacters < inputState->maxCharacters
            ? inputState->maxCharacters - usedCharacters
            : 0;

    const std::string pasted =
        sanitizeSingleLineClipboard(
            clipboard.getText(clipboard.userData),
            availableCharacters
        );

    if (pasted.empty()) {
      return;
    }

    deleteSelection(
        *textState,
        *inputState
    );

    textState->text.insert(
        inputState->cursor,
        pasted
    );

    inputState->cursor +=
        static_cast<u32>(pasted.size());

    inputState->selectionAnchor =
        inputState->cursor;

    touchTextInput(
        tree,
        handle,
        *inputState
    );

    ensureTextInputCaretVisible(
        tree,
        font,
        handle
    );

    return;
  }

  if (shortcut && event.key == Key::A) {
    inputState->selectionAnchor = 0;
    inputState->cursor = static_cast<u32>(textState->text.size());

    touchTextInput(tree, handle, *inputState);
    ensureTextInputCaretVisible(tree, font, handle);
    return;
  }

  switch (event.key) {
  case Key::Left: {
    u32 position = inputState->cursor;

    if (!selecting && hasSelection(*inputState)) {
      position = selectionStart(*inputState);
    } else {
      position = previousUtf8Offset(textState->text, inputState->cursor);
    }

    if (position != inputState->cursor ||
        (!selecting && hasSelection(*inputState))) {
      moveCursor(*inputState, position, selecting);
      touchTextInput(tree, handle, *inputState);
      ensureTextInputCaretVisible(tree, font, handle);
    }

    break;
  }

  case Key::Right: {
    u32 position = inputState->cursor;

    if (!selecting && hasSelection(*inputState)) {
      position = selectionEnd(*inputState);
    } else {
      position = nextUtf8Offset(textState->text, inputState->cursor);
    }

    if (position != inputState->cursor ||
        (!selecting && hasSelection(*inputState))) {
      moveCursor(*inputState, position, selecting);
      touchTextInput(tree, handle, *inputState);
      ensureTextInputCaretVisible(tree, font, handle);
    }

    break;
  }

  case Key::Home: {
    if (inputState->cursor != 0 || (!selecting && hasSelection(*inputState))) {
      moveCursor(*inputState, 0, selecting);
      touchTextInput(tree, handle, *inputState);
      ensureTextInputCaretVisible(tree, font, handle);
    }

    break;
  }

  case Key::End: {
    const u32 end = static_cast<u32>(textState->text.size());

    if (inputState->cursor != end ||
        (!selecting && hasSelection(*inputState))) {
      moveCursor(*inputState, end, selecting);
      touchTextInput(tree, handle, *inputState);
      ensureTextInputCaretVisible(tree, font, handle);
    }

    break;
  }

  case Key::Backspace: {
    if (inputState->readOnly) {
      break;
    }

    bool changed = deleteSelection(*textState, *inputState);

    if (!changed && inputState->cursor > 0) {
      const u32 previous =
          previousUtf8Offset(textState->text, inputState->cursor);

      textState->text.erase(previous, inputState->cursor - previous);

      inputState->cursor = previous;
      inputState->selectionAnchor = previous;

      changed = true;
    }

    if (changed) {
      touchTextInput(tree, handle, *inputState);
      ensureTextInputCaretVisible(tree, font, handle);
    }

    break;
  }

  case Key::Delete: {
    if (inputState->readOnly) {
      break;
    }

    bool changed = deleteSelection(*textState, *inputState);

    if (!changed &&
        inputState->cursor < static_cast<u32>(textState->text.size())) {
      const u32 next = nextUtf8Offset(textState->text, inputState->cursor);

      textState->text.erase(inputState->cursor, next - inputState->cursor);

      changed = true;
    }

    if (changed) {
      touchTextInput(tree, handle, *inputState);
      ensureTextInputCaretVisible(tree, font, handle);
    }

    break;
  }

  default:
    break;
  }
}

void WidgetSystem::beginTextInputSelection(Tree &tree, Font &font,
                                           NodeHandle handle,
                                           Vec2 mousePosition,
                                           bool extendSelection) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::TextInput) {
    return;
  }

  TextInputState *inputState = tree.textInputState(handle);
  const TextState *textState = tree.textState(handle);

  if (!inputState || !textState) {
    return;
  }

  const Rect content = detail::contentRect(tree, handle, tree.rect(handle));

  if (content.isEmpty()) {
    return;
  }

  f32 localX = mousePosition.x - content.x + inputState->scrollX;

  if (localX < 0.0f) {
    localX = 0.0f;
  }

  const u32 offset = font.textOffsetAtX(textState->text, localX);

  if (!extendSelection) {
    inputState->selectionAnchor = offset;
  }

  inputState->cursor = offset;
  inputState->draggingSelection = true;
  inputState->caretBlinkTimer = 0.0f;

  ensureTextInputCaretVisible(tree, font, handle);
  tree.markPaintDirty(handle);
}

void WidgetSystem::updateTextInputSelection(Tree &tree, Font &font,
                                            NodeHandle handle,
                                            Vec2 mousePosition, f32 deltaTime) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::TextInput) {
    return;
  }

  TextInputState *inputState = tree.textInputState(handle);
  const TextState *textState = tree.textState(handle);

  if (!inputState || !textState || !inputState->draggingSelection) {
    return;
  }

  const Rect content = detail::contentRect(tree, handle, tree.rect(handle));

  if (content.isEmpty()) {
    return;
  }

  if (deltaTime < 0.0f) {
    deltaTime = 0.0f;
  }

  f32 newScroll = inputState->scrollX;

  if (mousePosition.x < content.x) {
    newScroll -= detail::TextInputDragScrollSpeed * deltaTime;
  } else if (mousePosition.x > content.maxX()) {
    newScroll += detail::TextInputDragScrollSpeed * deltaTime;
  }

  const f32 maxScroll = maxTextInputScrollX(tree, font, handle);

  if (newScroll < 0.0f) {
    newScroll = 0.0f;
  }

  if (newScroll > maxScroll) {
    newScroll = maxScroll;
  }

  const bool scrollChanged = newScroll != inputState->scrollX;

  inputState->scrollX = newScroll;

  f32 mouseX = mousePosition.x;

  if (mouseX < content.x) {
    mouseX = content.x;
  }

  if (mouseX > content.maxX()) {
    mouseX = content.maxX();
  }

  const f32 localX = mouseX - content.x + inputState->scrollX;

  const u32 newCursor = font.textOffsetAtX(textState->text, localX);

  const bool cursorChanged = newCursor != inputState->cursor;

  inputState->cursor = newCursor;

  if (cursorChanged) {
    inputState->caretBlinkTimer = 0.0f;
  }

  if (cursorChanged || scrollChanged) {
    tree.markPaintDirty(handle);
  }
}

void WidgetSystem::endTextInputSelection(Tree &tree, NodeHandle handle) {
  TextInputState *state = tree.textInputState(handle);

  if (!state || !state->draggingSelection) {
    return;
  }

  state->draggingSelection = false;
}

void WidgetSystem::beginNumericInputDrag(Tree& tree, NodeHandle handle, Vec2 mousePosition) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::NumericInput) {
    return;
  }

  NumericInputState* state = tree.numericInputState(handle);
  TextInputState* inputState = tree.textInputState(handle);

  if (!state || !inputState) {
    return;
  }

  state->dragCandidate = false;
  state->dragging = false;

  if (inputState->readOnly || state->step <= 0.0) {
    return;
  }

  f64 startValue = state->value;
  f64 parsedValue = 0.0;

  if (parseNumericText(tree.text(handle), parsedValue)) {
    startValue = normalizeNumericValue(*state, parsedValue);
  }

  state->dragStartValue = startValue;
  state->dragStartX = mousePosition.x;

  state->dragCandidate = true;
}

bool WidgetSystem::updateNumericInputDrag(Tree& tree, EventSystem& events, NodeHandle handle, Vec2 mousePosition) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::NumericInput) {
    return false;
  }

  NumericInputState* state = tree.numericInputState(handle);
  TextInputState* inputState = tree.textInputState(handle);

  if (!state || !inputState) {
    return false;
  }

  if (inputState->readOnly || state->step <= 0.0) {
    return false;
  }

  if (!state->dragCandidate && !state->dragging) {
    return false;
  }

  const f32 deltaX = mousePosition.x - state->dragStartX;

  if (!state->dragging) {
    if (std::abs(deltaX) < detail::NumericInputDragThreshold) {
      return true;
    }

    state->dragging = true;
    state->dragCandidate = false;

    inputState->draggingSelection = false;

    syncNumericInputText(tree, handle);
  }

  const f64 steps =
      std::round(static_cast<f64>(deltaX) /
                 static_cast<f64>(detail::NumericInputDragPixelsPerStep));

  const f64 value =
      state->dragStartValue + steps * state->step;

  const f64 normalized =
      normalizeNumericValue(*state, value);

  if (normalized == state->value) {
    return true;
  }

  setNumericValue(tree, events, handle, normalized);

  return true;
}

void WidgetSystem::endNumericInputDrag(Tree& tree, NodeHandle handle) noexcept {
  NumericInputState* state = tree.numericInputState(handle);

  if (!state) {
    return;
  }

  state->dragCandidate = false;
  state->dragging = false;
}

void WidgetSystem::commitNumericInput(Tree& tree, EventSystem& events, NodeHandle handle) {
  if (!tree.isValid(handle) ||
      tree.type(handle) != NodeType::NumericInput) {
    return;
  }

  NumericInputState* state = tree.numericInputState(handle);

  if (!state) {
    return;
  }

  const f64 oldValue = state->value;

  f64 parsedValue = 0.0;

  if (parseNumericText(tree.text(handle), parsedValue)) {
    state->value =
        normalizeNumericValue(*state, parsedValue);
  }

  state->dragCandidate = false;
  state->dragging = false;

  syncNumericInputText(tree, handle);

  if (oldValue != state->value) {
    events.pushValueChanged(
        handle,
        oldValue,
        state->value);
  }
}

void WidgetSystem::updateTextInputVisualState(Tree& tree, NodeHandle handle,
                                              f32 deltaTime) {
  TextInputState* state =
      tree.textInputState(handle);

  if (!state || deltaTime <= 0.0f) {
    return;
  }

  const bool wasVisible =
      state->caretBlinkTimer <
      detail::TextInputCaretVisibleDuration;

  state->caretBlinkTimer += deltaTime;

  while (state->caretBlinkTimer >=
         detail::TextInputCaretBlinkPeriod) {
    state->caretBlinkTimer -=
        detail::TextInputCaretBlinkPeriod;
  }

  const bool isVisible =
      state->caretBlinkTimer <
      detail::TextInputCaretVisibleDuration;

  if (wasVisible != isVisible) {
    tree.markPaintDirty(handle);
  }
}

// NumericInput

NodeHandle WidgetSystem::createNumericInput(Tree& tree, StyleSystem& styles, NodeHandle parent, f64 value, NumericInputType type) {
  NodeHandle handle = createNode(tree, styles, NodeType::NumericInput, parent);

  tree.setInteractive(handle, true);
  tree.setFocusable(handle, true);
  tree.setTextAlign(handle, TextAlign::Start, TextAlign::Center);

  LayoutStyle* layout = tree.layoutStyle(handle);

  if (layout) {
    layout->preferredSize = Vec2{120.0f, 34.0f};
    layout->minSize = Vec2{60.0f, 28.0f};
    layout->padding = Padding{10.0f, 6.0f};
  }

  NumericInputState* numericState = tree.numericInputState(handle);

  if (!numericState) {
    return handle;
  }

  TextInputState* inputState = tree.textInputState(handle);

  if (inputState) {
    inputState->maxCharacters =
        DefaultNumericInputCharacterLimit;
  }

  TextState* textState = tree.textState(handle);

  if (textState) {
    textState->text.reserve(32);
  }

  numericState->type = type;
  numericState->step = type == NumericInputType::Integer ? 1.0 : 0.1;

  if (type == NumericInputType::Integer) {
    value = std::round(value);
  }

  numericState->value = normalizeNumericValue(*numericState, value);
  syncNumericInputText(tree, handle);

  return handle;
}

f64 WidgetSystem::numericValue(const Tree& tree, NodeHandle handle) const noexcept {
  const NumericInputState* state = tree.numericInputState(handle);

  if (!state) {
    return 0.0;
  }

  return state->value;
}

void WidgetSystem::syncNumericInputText(Tree& tree, NodeHandle handle) {
  NumericInputState* numericState = tree.numericInputState(handle);

  if (!numericState) {
    return;
  }

  const std::string text =
      formatNumericValue(numericState->value, numericState->type);

  tree.setText(handle, text);

  TextInputState* inputState = tree.textInputState(handle);

  if (!inputState) {
    return;
  }

  inputState->cursor = static_cast<u32>(text.size());
  inputState->selectionAnchor = inputState->cursor;
  inputState->scrollX = 0.0f;
  inputState->caretBlinkTimer = 0.0f;
  inputState->draggingSelection = false;
}

void WidgetSystem::adjustNumericInput(Tree& tree, EventSystem& events, NodeHandle handle, i32 direction) {
  NumericInputState* state = tree.numericInputState(handle);
  TextInputState* inputState = tree.textInputState(handle);

  if (!state || !inputState || inputState->readOnly || direction == 0) {
    return;
  }

  if (state->step <= 0.0) {
    return;
  }

  f64 baseValue = state->value;
  f64 parsedValue = 0.0;

  if (parseNumericText(tree.text(handle), parsedValue)) {
    baseValue = normalizeNumericValue(*state, parsedValue);
  }

  setNumericValue(
      tree,
      events,
      handle,
      baseValue + state->step * static_cast<f64>(direction));
}

void WidgetSystem::setNumericValue(Tree& tree, EventSystem& events, NodeHandle handle, f64 value) {
  NumericInputState* state = tree.numericInputState(handle);

  if (!state || !std::isfinite(value)) {
    return;
  }

  const f64 oldValue = state->value;
  const f64 newValue = normalizeNumericValue(*state, value);

  state->value = newValue;

  syncNumericInputText(tree, handle);

  if (oldValue != newValue) {
    events.pushValueChanged(handle, oldValue, newValue);
  }
}

void WidgetSystem::setNumericRange(Tree& tree, EventSystem& events, NodeHandle handle, f64 minValue, f64 maxValue) {
  NumericInputState* state = tree.numericInputState(handle);

  if (!state || std::isnan(minValue) || std::isnan(maxValue)) {
    return;
  }

  if (maxValue < minValue) {
    maxValue = minValue;
  }

  const f64 oldValue = state->value;

  state->minValue = minValue;
  state->maxValue = maxValue;
  state->value = normalizeNumericValue(*state, state->value);

  syncNumericInputText(tree, handle);

  if (oldValue != state->value) {
    events.pushValueChanged(handle, oldValue, state->value);
  }
}

void WidgetSystem::setNumericStep(Tree& tree, NodeHandle handle, f64 step) {
  NumericInputState* state = tree.numericInputState(handle);

  if (!state || !std::isfinite(step)) {
    return;
  }

  if (step < 0.0) {
    step = 0.0;
  }

  if (state->type == NumericInputType::Integer && step > 0.0) {
    step = std::round(step);

    if (step < 1.0) {
      step = 1.0;
    }
  }

  state->step = step;
}

void WidgetSystem::setNumericType(Tree& tree, EventSystem& events, NodeHandle handle, NumericInputType type) {
  NumericInputState* state = tree.numericInputState(handle);

  if (!state || state->type == type) {
    return;
  }

  const f64 oldValue = state->value;

  state->type = type;

  if (type == NumericInputType::Integer && state->step > 0.0) {
    state->step = std::round(state->step);

    if (state->step < 1.0) {
      state->step = 1.0;
    }
  }

  state->value = normalizeNumericValue(*state, state->value);

  syncNumericInputText(tree, handle);

  if (oldValue != state->value) {
    events.pushValueChanged(handle, oldValue, state->value);
  }
}

void WidgetSystem::handleNumericInputEvent(Tree& tree, Font& font, EventSystem& events, NodeHandle handle, const InputEvent& event, const ClipboardCallbacks& clipboard) {
  if (!tree.isValid(handle) || tree.type(handle) != NodeType::NumericInput) {
    return;
  }

  NumericInputState* numericState = tree.numericInputState(handle);
  TextInputState* inputState = tree.textInputState(handle);

  if (!numericState || !inputState) {
    return;
  }

  if (event.type == InputEventType::Key &&
      event.action == KeyAction::Press &&
      event.key == Key::Enter) {
    commitNumericInput(
        tree,
        events,
        handle);

    return;
  }

  if (event.type == InputEventType::Key &&
    (event.action == KeyAction::Press ||
     event.action == KeyAction::Repeat)) {
  if (event.key == Key::Up) {
    adjustNumericInput(tree, events, handle, 1);
    return;
  }

  if (event.key == Key::Down) {
    adjustNumericInput(tree, events, handle, -1);
    return;
  }
}

  const std::string previousText{tree.text(handle)};
  const TextInputState previousInputState = *inputState;

  handleTextInputEvent(tree, font, handle, event, clipboard);

  const std::string_view editedText = tree.text(handle);

  if (!isValidNumericEdit(editedText, numericState->type)) {
    tree.setText(handle, previousText);
    *inputState = previousInputState;

    tree.markPaintDirty(handle);
    ensureTextInputCaretVisible(tree, font, handle);
    return;
  }

  f64 parsedValue = 0.0;

  if (!parseNumericText(editedText, parsedValue)) {
    return;
  }

  if (parsedValue < numericState->minValue ||
      parsedValue > numericState->maxValue) {
    return;
  }

  const f64 newValue =
      normalizeNumericValue(*numericState, parsedValue);

  const f64 oldValue = numericState->value;

  if (oldValue == newValue) {
    return;
  }

  numericState->value = newValue;

  events.pushValueChanged(handle, oldValue, newValue);
}

void WidgetSystem::setTextInputCharacterLimit(Tree& tree, NodeHandle handle, u32 maxCharacters) {
  if (!tree.isValid(handle)) {
    return;
  }

  const NodeType type = tree.type(handle);

  if (type != NodeType::TextInput &&
      type != NodeType::NumericInput) {
    return;
  }

  TextInputState* state =
      tree.textInputState(handle);

  if (!state) {
    return;
  }

  state->maxCharacters = maxCharacters;

  std::string_view text = tree.text(handle);

  if (utf8CharacterCount(text) <= maxCharacters) {
    return;
  }

  const usize bytes =
      utf8PrefixBytes(text, maxCharacters);

  tree.setText(
      handle,
      text.substr(0, bytes));

  state->cursor =
      clampUtf8Offset(
          tree.text(handle),
          state->cursor);

  if (state->cursor > bytes) {
    state->cursor =
        static_cast<u32>(bytes);
  }

  state->selectionAnchor =
      clampUtf8Offset(
          tree.text(handle),
          state->selectionAnchor);

  if (state->selectionAnchor > bytes) {
    state->selectionAnchor =
        static_cast<u32>(bytes);
  }

  tree.markPaintDirty(handle);
}

u32 WidgetSystem::textInputCharacterLimit(const Tree& tree, NodeHandle handle) const noexcept {
  const TextInputState* state =
      tree.textInputState(handle);

  return state
      ? state->maxCharacters
      : 0;
}

} // namespace octogui
