#include <iterator>
#include <string_view>
#include <array>
#include <string>

#include "editorShortcuts.hpp"
namespace Cthulhu::Editor::Shortcuts
{
namespace
{
using namespace octogui;

struct Binding
{
    Action action;
    Key key;
    ModifierKeys modifiers{};
};

constexpr ModifierKeys Ctrl{.ctrl = true};
constexpr ModifierKeys CtrlShift{.shift = true, .ctrl = true};
constexpr ModifierKeys CtrlAlt{.ctrl = true, .alt = true};

// you may edit bindings here and Key::Unknown disables a binding and its label
constexpr Binding bindings[] = {
    {Action::Undo,                 Key::Z,      Ctrl},
    {Action::Redo,                 Key::Y,      Ctrl},
    {Action::NewScene,             Key::N,      Ctrl},
    {Action::OpenScene,            Key::O,      Ctrl},
    {Action::SaveScene,            Key::S,      Ctrl},
    {Action::SaveSceneAs,          Key::S,      CtrlShift},
    {Action::CloseScene,           Key::W,      Ctrl},
    {Action::CloseAllScenes,       Key::W,      CtrlShift},
    {Action::Quit,                 Key::Q,      Ctrl},

    {Action::Cut,                  Key::X,      Ctrl},
    {Action::Copy,                 Key::C,      Ctrl},
    {Action::Paste,                Key::V,      Ctrl},
    {Action::Duplicate,            Key::D,      Ctrl},
    {Action::Rename,               Key::F2},
    {Action::Delete,               Key::Delete},

    {Action::SelectTool,           Key::Q},
    {Action::MoveTool,             Key::W},
    {Action::RotateTool,           Key::E},
    {Action::ScaleTool,            Key::R},
    {Action::TogglePlay,           Key::F5},
    {Action::ToggleFullscreen,     Key::F11},
    {Action::ToggleVSync,          Key::F9},

    {Action::ResetLayout,          Key::Digit0, CtrlAlt},
    {Action::ToggleHierarchy,      Key::Digit1, CtrlAlt},
    {Action::ToggleInspector,      Key::Digit2, CtrlAlt},
    {Action::ToggleContentBrowser, Key::Digit3, CtrlAlt},
};

constexpr bool matches(const Binding &binding, const InputEvent &event)
{
    return binding.key != Key::Unknown &&
        event.type == InputEventType::Key &&
        event.action == KeyAction::Press &&
        event.key == binding.key && event.modifiers == binding.modifiers;
}

constexpr bool validBindings()
{
    if (std::size(bindings) != static_cast<std::size_t>(Action::Count) - 1)
        return false;

    for (std::size_t i = 0; i < std::size(bindings); ++i)
    {
        if (bindings[i].action <= Action::None || bindings[i].action >= Action::Count)
            return false;

        for (std::size_t j = 0; j < i; ++j)
        {
            if (bindings[i].action == bindings[j].action)
                return false;

            if (bindings[i].key != Key::Unknown &&
                bindings[i].key == bindings[j].key &&
                bindings[i].modifiers == bindings[j].modifiers)
                return false;
        }
    }
    return true;
}

static_assert(validBindings(), "Missing, repeated or conflicting editor shortcuts.");

static_assert([] {
    constexpr Binding save{Action::SaveScene, Key::S, Ctrl};
    InputEvent event{};
    event.key = Key::S;
    event.modifiers = Ctrl;
    if (!matches(save, event)) return false;

    event.modifiers.shift = true;
    if (matches(save, event)) return false;

    event.modifiers = Ctrl;
    event.modifiers.alt = true;
    if (matches(save, event)) return false;

    event.modifiers = Ctrl;
    event.modifiers.super = true;
    if (matches(save, event)) return false;

    event.modifiers = {};
    if (matches(save, event)) return false;

    event.modifiers = Ctrl;
    event.action = KeyAction::Repeat;
    if (matches(save, event)) return false;

    event.action = KeyAction::Release;
    if (matches(save, event)) return false;

    event.action = KeyAction::Press;
    event.type = InputEventType::Text;
    if (matches(save, event)) return false;

    event.type = InputEventType::Key;
    event.key = Key::Z;
    return !matches(save, event) &&
        !matches(Binding{Action::SaveScene, Key::Unknown}, InputEvent{});
}());

std::string keyLabel(Key key)
{
    const auto number = static_cast<unsigned>(key);

    if (key >= Key::A && key <= Key::Z)
        return std::string(1, static_cast<char>(
            'A' + number - static_cast<unsigned>(Key::A)));

    if (key >= Key::Digit0 && key <= Key::Digit9)
        return std::string(1, static_cast<char>(
            '0' + number - static_cast<unsigned>(Key::Digit0)));

    if (key >= Key::F1 && key <= Key::F25)
        return "F" + std::to_string(
            number - static_cast<unsigned>(Key::F1) + 1);

    if (key >= Key::Keypad0 && key <= Key::Keypad9)
        return "Numpad " + std::to_string(
            number - static_cast<unsigned>(Key::Keypad0));

    switch (key)
    {
    case Key::Unknown: return {};
    case Key::Space: return "Space";
    case Key::Apostrophe: return "'";
    case Key::Comma: return ",";
    case Key::Minus: return "-";
    case Key::Period: return ".";
    case Key::Slash: return "/";
    case Key::Semicolon: return ";";
    case Key::Equal: return "=";
    case Key::LeftBracket: return "[";
    case Key::Backslash: return "\\";
    case Key::RightBracket: return "]";
    case Key::GraveAccent: return "`";
    case Key::Escape: return "Esc";
    case Key::Enter: return "Enter";
    case Key::Tab: return "Tab";
    case Key::Backspace: return "Backspace";
    case Key::Insert: return "Insert";
    case Key::Delete: return "Delete";
    case Key::Right: return "Right";
    case Key::Left: return "Left";
    case Key::Down: return "Down";
    case Key::Up: return "Up";
    case Key::PageUp: return "Page Up";
    case Key::PageDown: return "Page Down";
    case Key::Home: return "Home";
    case Key::End: return "End";
    case Key::CapsLock: return "Caps Lock";
    case Key::ScrollLock: return "Scroll Lock";
    case Key::NumLock: return "Num Lock";
    case Key::PrintScreen: return "Print Screen";
    case Key::Pause: return "Pause";
    case Key::KeypadDecimal: return "Numpad .";
    case Key::KeypadDivide: return "Numpad /";
    case Key::KeypadMultiply: return "Numpad *";
    case Key::KeypadSubtract: return "Numpad -";
    case Key::KeypadAdd: return "Numpad +";
    case Key::KeypadEnter: return "Numpad Enter";
    case Key::KeypadEqual: return "Numpad =";
    case Key::Menu: return "Menu";
    default: return "Key " + std::to_string(number);
    }
}
}

Action read(const octogui::InputState &input) noexcept
{
    for (const auto &event : input.events)
        for (const auto &binding : bindings)
            if (matches(binding, event))
                return binding.action;

    return Action::None;
}

Action read(const octogui::Context &ctx) noexcept
{
    using namespace octogui;

    if (ctx.contextMenuOwnsInput())
        return Action::None;

    const auto &mouse = ctx.input().mouse;
    for (const auto button : {
        MouseButton::Left, MouseButton::Right, MouseButton::Middle})
    {
        if (mouse.isDown(button) || mouse.wasDown(button))
            return Action::None;
    }

    const Action action = read(ctx.input());
    const auto focus = ctx.focused();

    if (ctx.isValid(focus))
    {
        const auto *combo = ctx.tree().comboBoxState(focus);
        if (combo && combo->open)
            return Action::None;

        const auto type = ctx.tree().type(focus);
        if ((type == NodeType::TextInput || type == NodeType::NumericInput) &&
            action != Action::SaveScene && action != Action::SaveSceneAs)
            return Action::None;
    }

    return action;
}

std::string_view label(Action action)
{
    static const auto labels = [] {
        std::array<std::string, static_cast<std::size_t>(Action::Count)> result;

        for (const auto &binding : bindings)
        {
            if (binding.key == octogui::Key::Unknown) continue;

            auto &text = result.at(static_cast<std::size_t>(binding.action));
            if (binding.modifiers.ctrl) text += "Ctrl+";
            if (binding.modifiers.shift) text += "Shift+";
            if (binding.modifiers.alt) text += "Alt+";
            if (binding.modifiers.super) text += "Super+";
            text += keyLabel(binding.key);
        }
        return result;
    }();

    const auto index = static_cast<std::size_t>(action);
    return index < labels.size()
        ? std::string_view(labels[index])
        : std::string_view{};
}
}