#include <cstring>

#include <GLFW/glfw3.h>

#include "input.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Core
{

GLFWwindow *Input::windowHandle = nullptr;
bool Input::enabled = true;
bool Input::currentKeys[GLFW_KEY_LAST + 1] = {};
bool Input::previousKeys[GLFW_KEY_LAST + 1] = {};
bool Input::currentMouseButtons[GLFW_MOUSE_BUTTON_LAST + 1] = {};
bool Input::previousMouseButtons[GLFW_MOUSE_BUTTON_LAST + 1] = {};
bool Input::suppressedMouseButtons[GLFW_MOUSE_BUTTON_LAST + 1] = {};
float Input::mouseDeltaX{};
float Input::mouseDeltaY{};
float Input::scrollDeltaY{};
bool Input::firstMouse = true;
float Input::lastX{};
float Input::lastY{};

void Input::shutdown()
{
    windowHandle = nullptr;
    enabled = true;

    std::memset(currentKeys, 0, sizeof(currentKeys));
    std::memset(previousKeys, 0, sizeof(previousKeys));
    std::memset(currentMouseButtons, 0, sizeof(currentMouseButtons));
    std::memset(previousMouseButtons, 0, sizeof(previousMouseButtons));
    std::memset(suppressedMouseButtons, 0, sizeof(suppressedMouseButtons));

    mouseDeltaX = 0.0f;
    mouseDeltaY = 0.0f;
    scrollDeltaY = 0.0f;
    lastX = 0.0f;
    lastY = 0.0f;
    firstMouse = true;
}

void Input::init(GLFWwindow *window, glm::vec2 resolution)
{
    shutdown(); 

    windowHandle = window;
    lastX = resolution.x / 2.0f;
    lastY = resolution.y / 2.0f;

    glfwSetCursorPosCallback(windowHandle, mouse_callback);
    glfwSetMouseButtonCallback(windowHandle, mouse_button_callback);
    glfwSetScrollCallback(windowHandle, scroll_callback);
    Log::Print("Input system initialized successfully", "ENGINE", LogType::LOG_SUCCESS);
}

GLFWwindow *Input::getWindowHandle()
{
    return windowHandle;
}

bool Input::setEnabled(bool value) noexcept
{
    const bool previous = enabled;
    enabled = value;

    if (!enabled)
    {
        mouseDeltaX = 0.0f;
        mouseDeltaY = 0.0f;
        scrollDeltaY = 0.0f;
    }

    return previous;
}

void Input::suppressMouseButtonUntilRelease(int button) noexcept
{
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
        return;

    suppressedMouseButtons[button] = true;
}

void Input::update()
{
    if (!windowHandle)
    {
        return;
    }

    memcpy(previousKeys, currentKeys, sizeof(currentKeys));
    memcpy(previousMouseButtons, currentMouseButtons, sizeof(currentMouseButtons));

    const bool focused = glfwGetWindowAttrib(windowHandle, GLFW_FOCUSED) == GLFW_TRUE;

    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
    {
        currentKeys[key] = focused && glfwGetKey(windowHandle, key) == GLFW_PRESS;
    }

    for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
    {
        currentMouseButtons[button] = focused && glfwGetMouseButton(windowHandle, button) == GLFW_PRESS;

        if (!currentMouseButtons[button])
            suppressedMouseButtons[button] = false;
    }
}

bool Input::isKeyDown(int key)
{
    if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST)
        return false;
    return enabled && currentKeys[key];
}

bool Input::isKeyPressed(int key)
{
    if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST)
        return false;
    return enabled && currentKeys[key] && !previousKeys[key];
}

bool Input::isKeyReleased(int key)
{
    if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST)
        return false;
    return enabled && previousKeys[key] && !currentKeys[key];
}

bool Input::isMouseButtonDown(int button)
{
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
        return false;
   return enabled && !suppressedMouseButtons[button] && currentMouseButtons[button];
}

bool Input::isMouseButtonPressed(int button)
{
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
        return false;
    return enabled && !suppressedMouseButtons[button] && currentMouseButtons[button] && !previousMouseButtons[button];
}

void Input::mouse_callback([[maybe_unused]] GLFWwindow *window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    mouseDeltaX += xoffset;
    mouseDeltaY += yoffset;
    lastX = xpos;
    lastY = ypos;
}

void Input::mouse_button_callback(GLFWwindow *window, int button, int action, [[maybe_unused]] int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void Input::scroll_callback([[maybe_unused]] GLFWwindow *window,
                            [[maybe_unused]] double xoffset,
                            [[maybe_unused]] double yoffset)
{
    scrollDeltaY += static_cast<float>(yoffset);
}

float Input::getScrollDeltaY()
{
    float delta = scrollDeltaY;
    scrollDeltaY = 0.0f;
    return enabled ? delta : 0.0f;
}

float Input::getMouseX()
{
   return enabled ? lastX : 0.0f;
}

float Input::getMouseY()
{
    return enabled ? lastY : 0.0f;
}

float Input::getMouseDeltaX()
{
    float delta = mouseDeltaX;
    mouseDeltaX = 0.0f;
    return enabled ? delta : 0.0f;
}

float Input::getMouseDeltaY()
{
    float delta = mouseDeltaY;
    mouseDeltaY = 0.0f;
    return enabled ? delta : 0.0f;
}

void Input::resetMouseTracking()
{
    if (windowHandle)
    {
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(windowHandle, &x, &y);
        lastX = static_cast<float>(x);
        lastY = static_cast<float>(y);
    }

    mouseDeltaX = 0.0f;
    mouseDeltaY = 0.0f;
    firstMouse = false;
}

} // namespace Cthulhu::Core
