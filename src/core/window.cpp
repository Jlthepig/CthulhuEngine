#include <memory>
#include <utility>

#include <GLFW/glfw3.h>

#include "camera.hpp"
#include "log_utils.hpp"
#include "window.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Core
{
static std::vector<unique_ptr<Window>> windowContainer;

Window::~Window()
{
    if (glfWwindow)
        glfwDestroyWindow(glfWwindow);
}

Window *Window::createWindow(const WindowConfig &config, const char *windowTitle, const Window *centerOver)
{
    const auto validDimension = [](float value) {
        return value >= 1.0f && static_cast<double>(value) <= std::numeric_limits<int>::max();
    };

    if (!validDimension(config.resolution.x) || !validDimension(config.resolution.y))
    {
        Log::Print("INVALID WINDOW SIZE", "Window", LogType::LOG_ERROR);
        return nullptr;
    }

    auto window = std::make_unique<Window>();
    GLFWwindow *previous = glfwGetCurrentContext();

    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    window->glfWwindow = glfwCreateWindow(static_cast<int>(config.resolution.x), static_cast<int>(config.resolution.y),
                                         windowTitle, nullptr, nullptr);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    if (!window->glfWwindow)
    {
        Log::Print("FAILED TO CREATE A WINDOW", "Window", LogType::LOG_ERROR);
        return nullptr;
    }

    GLFWwindow *native = window->glfWwindow;
    glfwMakeContextCurrent(native);
    if (glfwGetCurrentContext() != native)
    {
        glfwMakeContextCurrent(previous);
        Log::Print("FAILED TO ACTIVATE THE WINDOW CONTEXT", "Window", LogType::LOG_ERROR);
        return nullptr;
    }

    if (centerOver)
        window->centerOn(*centerOver);

    int width = 0, height = 0;
    glfwGetWindowSize(native, &width, &height);
    window->windowSize = {static_cast<float>(width), static_cast<float>(height)};

    if (config.minimumSize.x > 0 || config.minimumSize.y > 0)
    {
        const int minWidth = config.minimumSize.x > 0 ? std::min(config.minimumSize.x, width) : GLFW_DONT_CARE;
        const int minHeight = config.minimumSize.y > 0 ? std::min(config.minimumSize.y, height) : GLFW_DONT_CARE;
        glfwSetWindowSizeLimits(native, minWidth, minHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }

    glfwSwapInterval(config.vSync ? 1 : 0);
    glfwSetWindowUserPointer(native, window.get());
    return windowContainer.emplace_back(std::move(window)).get();
}

void Window::centerOn(const Window &parent)
{
    int parentX = 0, parentY = 0, parentWidth = 0, parentHeight = 0;
    glfwGetWindowPos(parent.glfWwindow, &parentX, &parentY);
    glfwGetWindowSize(parent.glfWwindow, &parentWidth, &parentHeight);

    int width = 0, height = 0;
    glfwGetWindowSize(glfWwindow, &width, &height);

    int workX = 0, workY = 0, workWidth = 0, workHeight = 0;
    if (auto *monitor = parent.getCurrentMonitor())
        glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);

    int x = parentX + (parentWidth - width) / 2;
    int y = parentY + (parentHeight - height) / 2;

    if (workWidth > 0 && workHeight > 0)
    {
        int left = 0, top = 0, right = 0, bottom = 0;
        glfwGetWindowFrameSize(glfWwindow, &left, &top, &right, &bottom);

        const int fittedWidth = std::min(width, std::max(1, workWidth - left - right));
        const int fittedHeight = std::min(height, std::max(1, workHeight - top - bottom));
        if (fittedWidth != width || fittedHeight != height)
            glfwSetWindowSize(glfWwindow, fittedWidth, fittedHeight);

        const int minX = workX + left;
        const int minY = workY + top;
        x = std::clamp(parentX + (parentWidth - fittedWidth) / 2, minX,
                       std::max(minX, workX + workWidth - right - fittedWidth));
        y = std::clamp(parentY + (parentHeight - fittedHeight) / 2, minY,
                       std::max(minY, workY + workHeight - bottom - fittedHeight));
    }

    glfwSetWindowPos(glfWwindow, x, y);
}

void Window::show()
{
    glfwShowWindow(glfWwindow);
}

void Window::focus()
{
    if (glfwGetWindowAttrib(glfWwindow, GLFW_ICONIFIED))
        glfwRestoreWindow(glfWwindow);
    glfwFocusWindow(glfWwindow);
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(glfWwindow) == GLFW_TRUE;
}

void Window::setWindowMode(WindowMode newMode)
{
    if (newMode == currentMode)
        return;
    if (currentMode == WindowMode::Windowed)
    {
        glfwGetWindowPos(glfWwindow, &cachedX, &cachedY);
        glfwGetWindowSize(glfWwindow, &cachedWidth, &cachedHeight);
    }
    switch (newMode)
    {
        case WindowMode::Windowed: {
            glfwSetWindowMonitor(glfWwindow, nullptr, cachedX, cachedY, cachedWidth, cachedHeight, 0);
            glfwSetWindowAttrib(glfWwindow, GLFW_DECORATED, GLFW_TRUE);
            // update state
            windowSize = glm::vec2(cachedWidth, cachedHeight);
            break;
        }
        case WindowMode::Borderless: {
            GLFWmonitor *monitor = getCurrentMonitor();
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(glfWwindow, monitor, 0, 0, mode->width, mode->height, 0);
            glfwSetWindowAttrib(glfWwindow, GLFW_DECORATED, GLFW_FALSE);
            // update state
            windowSize = glm::vec2(mode->width, mode->height);
            break;
        }
        case WindowMode::ExclusiveFullscreen: {
            GLFWmonitor *monitor = getCurrentMonitor();
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(glfWwindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            // update state
            windowSize = glm::vec2(mode->width, mode->height);
            break;
        }
    }
    currentMode = newMode;
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(glfWwindow, &fbWidth, &fbHeight);
}

float Window::getWidth()
{
    return windowSize.x;
}

float Window::getHeight()
{
    return windowSize.y;
}

GLFWwindow *Window::getWindow() const
{
    return glfWwindow;
}

GLFWmonitor *Window::getCurrentMonitor() const
{
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    int winX, winY, winW, winH;
    glfwGetWindowPos(glfWwindow, &winX, &winY);
    glfwGetWindowSize(glfWwindow, &winW, &winH);

    int centerX = winX + winW / 2;
    int centerY = winY + winH / 2;
    if (monitorCount > 0)
    {
        for (int i = 0; i < monitorCount; ++i)
        {
            int xPos, yPos;
            glfwGetMonitorPos(monitors[i], &xPos, &yPos);
            const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
            if (mode && centerX >= xPos && centerX < xPos + mode->width && centerY >= yPos && centerY < yPos + mode->height)
            {
                return monitors[i];
            }
        }
    }

    return glfwGetPrimaryMonitor();
}

void Window::destroyWindow(Window *window)
{
    const auto it = std::find_if(windowContainer.begin(), windowContainer.end(), [window](const auto &owned) { return owned.get() == window; });
    if (it != windowContainer.end())
        windowContainer.erase(it);
}

void Window::destroyAll()
{
    windowContainer.clear();
}

} // namespace Cthulhu::Core
