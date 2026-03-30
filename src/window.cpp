
#include "window.h"
#include "log_utils.hpp"
#include "camera.h"
#include <memory>
#include <utility>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Core
{
    static std::vector<unique_ptr<Window>> windowContainer;
    Window* Window::createWindow(glm::vec2 windowSize,const char* windowTitle)
    {
        unique_ptr<Window> window = std::make_unique<Window>();
         window->glfWwindow = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle, NULL, NULL);

        if (!window) {  Log::Print("FAILED TO CREATE A WINDOW.", "Main", LogType::LOG_ERROR); glfwTerminate(); return nullptr; }
        Window* window_ptr = window.get();
        window->windowSize = windowSize;
        glfwMakeContextCurrent( window->glfWwindow);
        glfwSetWindowUserPointer(window->glfWwindow, window_ptr);  // attach this instance
        glfwSetFramebufferSizeCallback( window->glfWwindow, framebuffer_size_callback);
        glfwSetCursorPosCallback( window->glfWwindow, mouse_callback);
        glfwSetMouseButtonCallback( window->glfWwindow, mouse_button_callback);
        
        windowContainer.push_back(std::move(window));
        return window_ptr;
    }
    
    void Window::setCamera(Cthulhu::Scene::Camera* cam)
    {
        camera = cam;
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
    
    void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
    }
    
    void Window::mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
    {   
        
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));

        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (self->firstMouse)
        {
            self->lastX = xpos;
            self->lastY = ypos;
            self->firstMouse = false;

        }

        float xoffset = xpos - self->lastX;
        float yoffset = self->lastY - ypos;
        self->lastX = xpos;
        self->lastY = ypos;

       if (self->camera)
       {
            Log::Print("mouse call log", "Main", LogType::LOG_INFO);
            self->camera->processMouse(xoffset,yoffset);
       }
    }
    
    void Window::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    
  

    
}