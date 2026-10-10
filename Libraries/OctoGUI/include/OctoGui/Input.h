#pragma once

#include <array>
#include <vector>
#include <string_view>

#include "OctoGui/Types.h"
#include "OctoGui/Vec2.h"

namespace octogui {
enum class MouseButton : u8 { Left = 0, Right = 1, Middle = 2, Count = 3 };

inline constexpr usize MouseButtonCount =
    static_cast<usize>(MouseButton::Count);

struct ModifierKeys {
  bool shift = false;
  bool ctrl = false;
  bool alt = false;
  bool super = false;

  [[nodiscard]]
  constexpr bool any() const noexcept {
    return shift || ctrl || alt || super;
  }

  constexpr void clear() noexcept {
    shift = false;
    ctrl = false;
    alt = false;
    super = false;
  }

  friend constexpr bool operator==(const ModifierKeys &lhs,
                                   const ModifierKeys &rhs) {
    return lhs.shift == rhs.shift && lhs.ctrl == rhs.ctrl &&
           lhs.alt == rhs.alt && lhs.super == rhs.super;
  }

  friend constexpr bool operator!=(const ModifierKeys &lhs,
                                   const ModifierKeys &rhs) {
    return !(lhs == rhs);
  }
};

enum class KeyAction : u8 { Press, Release, Repeat };

enum class Key : u16 {
  Unknown,

  Space,
  Apostrophe,
  Comma,
  Minus,
  Period,
  Slash,

  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,

  Semicolon,
  Equal,

  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,

  LeftBracket,
  Backslash,
  RightBracket,
  GraveAccent,

  World1,
  World2,

  Escape,
  Enter,
  Tab,
  Backspace,
  Insert,
  Delete,

  Right,
  Left,
  Down,
  Up,

  PageUp,
  PageDown,
  Home,
  End,

  CapsLock,
  ScrollLock,
  NumLock,
  PrintScreen,
  Pause,

  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  F13,
  F14,
  F15,
  F16,
  F17,
  F18,
  F19,
  F20,
  F21,
  F22,
  F23,
  F24,
  F25,

  Keypad0,
  Keypad1,
  Keypad2,
  Keypad3,
  Keypad4,
  Keypad5,
  Keypad6,
  Keypad7,
  Keypad8,
  Keypad9,

  KeypadDecimal,
  KeypadDivide,
  KeypadMultiply,
  KeypadSubtract,
  KeypadAdd,
  KeypadEnter,
  KeypadEqual,

  LeftShift,
  LeftControl,
  LeftAlt,
  LeftSuper,

  RightShift,
  RightControl,
  RightAlt,
  RightSuper,

  Menu
};

enum class InputEventType : u8 { Key, Text };

struct InputEvent {
  InputEventType type = InputEventType::Key;

  Key key = Key::Unknown;
  KeyAction action = KeyAction::Press;

  i32 scanCode = 0;
  u32 codepoint = 0;

  ModifierKeys modifiers{};
};
struct MouseState {
  Vec2 position{};
  Vec2 previousPosition{};
  Vec2 wheel{};

  std::array<bool, MouseButtonCount> down{};
  std::array<bool, MouseButtonCount> previousDown{};

  [[nodiscard]]
  static constexpr usize indexOf(MouseButton button) noexcept {
    return static_cast<usize>(button);
  }

  [[nodiscard]]
  constexpr bool isDown(MouseButton button) const noexcept {
    return down[indexOf(button)];
  }

  [[nodiscard]]
  constexpr bool wasDown(MouseButton button) const noexcept {
    return previousDown[indexOf(button)];
  }

  [[nodiscard]]
  constexpr bool pressed(MouseButton button) const noexcept {
    return isDown(button) && !wasDown(button);
  }

  [[nodiscard]]
  constexpr bool released(MouseButton button) const noexcept {
    return !isDown(button) && wasDown(button);
  }

  [[nodiscard]]
  constexpr Vec2 delta() const noexcept {
    return position - previousPosition;
  }

  [[nodiscard]]
  constexpr bool moved() const noexcept {
    return delta() != Vec2::zero();
  }

  [[nodiscard]]
  constexpr bool dragged(MouseButton button) const noexcept {
    return isDown(button) && moved();
  }

  constexpr void setDown(MouseButton button, bool value) noexcept {
    down[indexOf(button)] = value;
  }

  constexpr void press(MouseButton button) noexcept { setDown(button, true); }

  constexpr void release(MouseButton button) noexcept {
    setDown(button, false);
  }

  constexpr void endFrame() noexcept {
    previousPosition = position;
    previousDown = down;
    wheel = Vec2::zero();
  }
};
struct ClipboardCallbacks {
  void* userData = nullptr;

  std::string_view (*getText)(void*) = nullptr;
  void (*setText)(void*, std::string_view) = nullptr;
};
struct InputState {
  f32 deltaTime = 0.0f;
  Vec2 displaySize{};
  f32 dpiScale = 1.0f;

  MouseState mouse{};
  ModifierKeys modifiers{};

  std::vector<InputEvent> events;

  ClipboardCallbacks clipboard{};

  constexpr void endFrame() noexcept {
    mouse.endFrame();
    events.clear();
  }
};
} // namespace octogui