
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
    Window* Window::createWindow(glm::vec2 windowSize,const char* windowTitle)
    {
        unique_ptr<Window> window = std::make_unique<Window>();
         window->glfWwindow = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle, NULL, NULL);

        if (!window) {  Log::Print("FAILED TO CREATE A WINDOW.", "Main", LogType::LOG_ERROR); glfwTerminate(); return nullptr; }
        Window* window_ptr = window.get();
        window->windowSize = windowSize;
        glfwMakeContextCurrent( window->glfWwindow);
        glfwSwapInterval(0);
        glfwSetWindowUserPointer(window->glfWwindow, window_ptr);  // attach this instance
        glfwSetFramebufferSizeCallback( window->glfWwindow, framebuffer_size_callback);
        
        
        
        windowContainer.push_back(std::move(window));
        return window_ptr;
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
    
   
    
   
    
  

    
}