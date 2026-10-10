#include "OctoGui_GLFW.h"

#include <unordered_map>
#include <vector>
#include <string>
namespace octogui 

{

namespace glfw 

{

struct WindowInputState {
    Vec2 pendingScroll = Vec2::zero();

    std::vector<InputEvent> pendingEvents;

    ModifierKeys lastModifiers{};

    GLFWscrollfun previousScrollCallback = nullptr;
    GLFWkeyfun previousKeyCallback = nullptr;
    GLFWcharfun previousCharCallback = nullptr;
};

std::unordered_map<GLFWwindow*, WindowInputState> inputStates;

ModifierKeys modifiersFromGlfw(int mods) noexcept {
    ModifierKeys result;

    result.shift = (mods & GLFW_MOD_SHIFT) != 0;
    result.ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    result.alt = (mods & GLFW_MOD_ALT) != 0;
    result.super = (mods & GLFW_MOD_SUPER) != 0;

    return result;
}

ModifierKeys queryModifiers(GLFWwindow* window) noexcept {
    ModifierKeys result;

    result.shift =
        glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    result.ctrl =
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    result.alt =
        glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

    result.super =
        glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;

    return result;
}

Key keyFromGlfw(int key) noexcept {
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        return static_cast<Key>(
            static_cast<u16>(Key::Digit0) +
            static_cast<u16>(key - GLFW_KEY_0)
        );
    }

    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        return static_cast<Key>(
            static_cast<u16>(Key::A) +
            static_cast<u16>(key - GLFW_KEY_A)
        );
    }

    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) {
        return static_cast<Key>(
            static_cast<u16>(Key::F1) +
            static_cast<u16>(key - GLFW_KEY_F1)
        );
    }

    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
        return static_cast<Key>(
            static_cast<u16>(Key::Keypad0) +
            static_cast<u16>(key - GLFW_KEY_KP_0)
        );
    }

    switch (key) {
    case GLFW_KEY_SPACE: return Key::Space;
    case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
    case GLFW_KEY_COMMA: return Key::Comma;
    case GLFW_KEY_MINUS: return Key::Minus;
    case GLFW_KEY_PERIOD: return Key::Period;
    case GLFW_KEY_SLASH: return Key::Slash;
    case GLFW_KEY_SEMICOLON: return Key::Semicolon;
    case GLFW_KEY_EQUAL: return Key::Equal;
    case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
    case GLFW_KEY_BACKSLASH: return Key::Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return Key::GraveAccent;

    case GLFW_KEY_WORLD_1: return Key::World1;
    case GLFW_KEY_WORLD_2: return Key::World2;

    case GLFW_KEY_ESCAPE: return Key::Escape;
    case GLFW_KEY_ENTER: return Key::Enter;
    case GLFW_KEY_TAB: return Key::Tab;
    case GLFW_KEY_BACKSPACE: return Key::Backspace;
    case GLFW_KEY_INSERT: return Key::Insert;
    case GLFW_KEY_DELETE: return Key::Delete;

    case GLFW_KEY_RIGHT: return Key::Right;
    case GLFW_KEY_LEFT: return Key::Left;
    case GLFW_KEY_DOWN: return Key::Down;
    case GLFW_KEY_UP: return Key::Up;

    case GLFW_KEY_PAGE_UP: return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
    case GLFW_KEY_HOME: return Key::Home;
    case GLFW_KEY_END: return Key::End;

    case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
    case GLFW_KEY_SCROLL_LOCK: return Key::ScrollLock;
    case GLFW_KEY_NUM_LOCK: return Key::NumLock;
    case GLFW_KEY_PRINT_SCREEN: return Key::PrintScreen;
    case GLFW_KEY_PAUSE: return Key::Pause;

    case GLFW_KEY_KP_DECIMAL: return Key::KeypadDecimal;
    case GLFW_KEY_KP_DIVIDE: return Key::KeypadDivide;
    case GLFW_KEY_KP_MULTIPLY: return Key::KeypadMultiply;
    case GLFW_KEY_KP_SUBTRACT: return Key::KeypadSubtract;
    case GLFW_KEY_KP_ADD: return Key::KeypadAdd;
    case GLFW_KEY_KP_ENTER: return Key::KeypadEnter;
    case GLFW_KEY_KP_EQUAL: return Key::KeypadEqual;

    case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
    case GLFW_KEY_LEFT_CONTROL: return Key::LeftControl;
    case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
    case GLFW_KEY_LEFT_SUPER: return Key::LeftSuper;

    case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
    case GLFW_KEY_RIGHT_CONTROL: return Key::RightControl;
    case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
    case GLFW_KEY_RIGHT_SUPER: return Key::RightSuper;

    case GLFW_KEY_MENU: return Key::Menu;

    default:
        return Key::Unknown;
    }
}

KeyAction keyActionFromGlfw(int action) noexcept {
    if (action == GLFW_RELEASE) {return KeyAction::Release;}
    if (action == GLFW_REPEAT) {return KeyAction::Repeat;}

    return KeyAction::Press;
}

std::string_view clipboardGetText(void* userData) {
    GLFWwindow* window = static_cast<GLFWwindow*>(userData);

    if (!window) {return {};}

    const char* text = glfwGetClipboardString(window);

    if (!text) {return {};}

    return std::string_view{text};
}

void clipboardSetText(void* userData, std::string_view text) {
    GLFWwindow* window = static_cast<GLFWwindow*>(userData);

    if (!window) {return;}

    const std::string nullTerminated{text};

    glfwSetClipboardString(
        window,
        nullTerminated.c_str()
    );
}

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    auto it = inputStates.find(window);

    if (it == inputStates.end()) {return;}

    WindowInputState& state = it->second;

    state.pendingScroll.x += static_cast<f32>(xOffset);
    state.pendingScroll.y += static_cast<f32>(yOffset);

    if (state.previousScrollCallback) {
        state.previousScrollCallback(window, xOffset, yOffset);
    }
}

void keyCallback(GLFWwindow* window, int key, int scanCode, int action, int mods) {
    auto it = inputStates.find(window);

    if (it == inputStates.end()) {return;}

    WindowInputState& state = it->second;

    InputEvent event;
    event.type = InputEventType::Key;
    event.key = keyFromGlfw(key);
    event.action = keyActionFromGlfw(action);
    event.scanCode = scanCode;
    event.modifiers = modifiersFromGlfw(mods);

    state.lastModifiers = event.modifiers;
    state.pendingEvents.push_back(event);

    if (state.previousKeyCallback) {
        state.previousKeyCallback(window, key, scanCode, action, mods);
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto it = inputStates.find(window);

    if (it == inputStates.end()) {return;}

    WindowInputState& state = it->second;

    InputEvent event;
    event.type = InputEventType::Text;
    event.codepoint = static_cast<u32>(codepoint);
    event.modifiers = state.lastModifiers;

    state.pendingEvents.push_back(event);

    if (state.previousCharCallback) {
        state.previousCharCallback(window, codepoint);
    }
}

void initInput(GLFWwindow* window) {
    if (!window || inputStates.contains(window)) {return;}

    WindowInputState& state =
        inputStates.emplace(window, WindowInputState{}).first->second;

    state.pendingEvents.reserve(32);

    state.previousScrollCallback =
        glfwSetScrollCallback(window, scrollCallback);

    state.previousKeyCallback =
        glfwSetKeyCallback(window, keyCallback);

    state.previousCharCallback =
        glfwSetCharCallback(window, charCallback);
}

void shutdownInput(GLFWwindow* window) {
    if (!window) {return;}

    auto it = inputStates.find(window);

    if (it == inputStates.end()) {return;}

    WindowInputState& state = it->second;

    glfwSetCharCallback(window, state.previousCharCallback);
    glfwSetKeyCallback(window, state.previousKeyCallback);
    glfwSetScrollCallback(window, state.previousScrollCallback);

    inputStates.erase(it);
}

void updateInput(Context& ctx, GLFWwindow* window) {
    if (!window) {
        return;
    }

    int width = 0;
    int height = 0;

    glfwGetWindowSize(window, &width, &height);

    ctx.input().displaySize = Vec2{
        static_cast<f32>(width),
        static_cast<f32>(height)
    };

    double mouseX = 0.0;
    double mouseY = 0.0;

    glfwGetCursorPos(window, &mouseX, &mouseY);

    ctx.input().mouse.position = Vec2{
        static_cast<f32>(mouseX),
        static_cast<f32>(mouseY)
    };

    ctx.input().mouse.setDown(
        MouseButton::Left,
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
    );

    ctx.input().mouse.setDown(
        MouseButton::Right,
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
    );

    ctx.input().mouse.setDown(
        MouseButton::Middle,
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS
    );

    InputState& input = ctx.input();

    auto inputIt = inputStates.find(window);

    input.events.clear();

    if (inputIt != inputStates.end()) {
        WindowInputState& state = inputIt->second;

        input.mouse.wheel = state.pendingScroll;
        state.pendingScroll = Vec2::zero();

        input.events.insert(
            input.events.end(),
            state.pendingEvents.begin(),
            state.pendingEvents.end()
        );

        state.pendingEvents.clear();
    } else {
        input.mouse.wheel = Vec2::zero();
    }

    input.modifiers = queryModifiers(window);

    if (inputIt != inputStates.end()) {
        inputIt->second.lastModifiers = input.modifiers;
    }

    input.dpiScale = 1.0f;

    ctx.input().clipboard.userData = window;
    ctx.input().clipboard.getText = clipboardGetText;
    ctx.input().clipboard.setText = clipboardSetText;
}

} // glfw
} // octogui