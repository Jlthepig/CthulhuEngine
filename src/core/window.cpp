
#include "pch.h"
#include "window.h"
#include "glfw3.h"
#include "log_utils.hpp"
#include "camera.h"
#include <memory>
#include <utility>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Core
{
    static std::vector<unique_ptr<Window>> windowContainer;
    Window* Window::createWindow(const WindowConfig& config, const char* windowTitle)
    {
        unique_ptr<Window> window = std::make_unique<Window>();
         window->glfWwindow = glfwCreateWindow((int)config.resolution.x, (int)config.resolution.y, windowTitle, NULL, NULL);

        if (!window->glfWwindow) {  Log::Print("FAILED TO CREATE A WINDOW.", "Main", LogType::LOG_ERROR); glfwTerminate(); return nullptr; }
        Window* window_ptr = window.get();
        window->windowSize = config.resolution;
        glfwMakeContextCurrent( window->glfWwindow);
        glfwSwapInterval(config.vSync ? 1 : 0); // Enable or disable V-Sync based on config
        glfwSetWindowUserPointer(window->glfWwindow, window_ptr);  // attach this instance
        glfwSetFramebufferSizeCallback( window->glfWwindow, framebuffer_size_callback);

        windowContainer.push_back(std::move(window));
        return window_ptr;
    }

    void Window::setWindowMode(WindowMode newMode)
    {
        if (newMode == currentMode) return;
        if (currentMode == WindowMode::Windowed)
        {
            glfwGetWindowPos(glfWwindow, &cachedX, &cachedY);
            glfwGetWindowSize(glfWwindow, &cachedWidth, &cachedHeight);
        }
        switch (newMode)
        {
            case WindowMode::Windowed:
            {
                glfwSetWindowMonitor(glfWwindow, nullptr, cachedX, cachedY, cachedWidth, cachedHeight, 0);
                glfwSetWindowAttrib(glfWwindow, GLFW_DECORATED, GLFW_TRUE);
                // update state
                windowSize = glm::vec2(cachedWidth, cachedHeight);
                break;
            }
            case WindowMode::Borderless:
            {
                GLFWmonitor* monitor = getCurrentMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(glfWwindow, monitor, 0, 0, mode->width, mode->height, 0);
                glfwSetWindowAttrib(glfWwindow, GLFW_DECORATED, GLFW_FALSE);
                // update state
                windowSize = glm::vec2(mode->width, mode->height);
                break;
            }
            case WindowMode::ExclusiveFullscreen:
            {
                GLFWmonitor* monitor = getCurrentMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(glfWwindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                // update state
                windowSize = glm::vec2(mode->width, mode->height);
                break;
            }
        }
        currentMode = newMode;
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(glfWwindow, &fbWidth, &fbHeight);
        framebuffer_size_callback(glfWwindow, fbWidth, fbHeight);
    }
    
    float Window::getWidth()
    {
        return windowSize.x;
    }
    
    float Window::getHeight()
    {
        return windowSize.y;
    }

    GLFWwindow* Window::getWindow() const
    {
        return glfWwindow;
    }   
    
    GLFWmonitor* Window::getCurrentMonitor() const
    {
        int monitorCount;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

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
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
                if (centerX >= xPos && centerX < xPos + mode->width &&
                    centerY >= yPos && centerY < yPos + mode->height)
                {
                    return monitors[i];
                }
            }
        }
        
        return glfwGetPrimaryMonitor();
    }

    void Window::framebuffer_size_callback([[maybe_unused]] GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
    }
}