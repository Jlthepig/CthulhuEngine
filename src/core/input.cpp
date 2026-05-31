#include "input.h"
#include "camera.h"
#include "glfw3.h"
#include <cstring>
#include <vector>

namespace Cthulhu::Core
{
    GLFWwindow* Input::windowHandle = nullptr; 
    bool Input::currentKeys[GLFW_KEY_LAST + 1] = {};
    bool Input::previousKeys[GLFW_KEY_LAST + 1] = {};
    float Input::mouseDeltaX = 0.0f;
    float Input::mouseDeltaY = 0.0f;
    bool Input::firstMouse = true;
    float Input::lastX = 0.0f;
    float Input::lastY = 0.0f;
    void Input::init(GLFWwindow* window,glm::vec2 resolution)
    {
        windowHandle = window;
        lastX = resolution.x/2.0f;
        lastY = resolution.y/2.0f;
        
        glfwSetCursorPosCallback(windowHandle,mouse_callback);
        glfwSetMouseButtonCallback(windowHandle, mouse_button_callback);
    }
    
    GLFWwindow* Input::getWindowHandle()
    {
        return windowHandle;
    }
    
    void Input::update()
    {
        memcpy(previousKeys, currentKeys, sizeof(currentKeys));

        for (int i = 0; i<= GLFW_KEY_LAST;i++)
        {
            currentKeys[i] = glfwGetKey(windowHandle, i) == GLFW_PRESS;
        }
        
    }
    
    bool Input::isKeyDown(int key)
    {
        return currentKeys[key];
    }

    bool Input::isKeyPressed(int key)
    {
        return currentKeys[key] && !previousKeys[key];
    }
    
    bool Input::isKeyReleased(int key)
    {
        return previousKeys[key] && !currentKeys[key];
    }
    
    void Input::mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
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
        mouseDeltaX = xoffset;
        mouseDeltaY = yoffset;
        lastX = xpos;
        lastY = ypos;
    }
    
    void Input::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    
    float Input::getMouseX()
    {
        return lastX;
    }

    float Input::getMouseY()
    {
        return lastY;
    }
    
    float Input::getMouseDeltaX()
    {
        float delta = mouseDeltaX;
        mouseDeltaX = 0.0f; 
        return delta;
    }
    
    float Input::getMouseDeltaY()
    {
        float delta = mouseDeltaY;
        mouseDeltaY = 0.0f;
        return delta;
    }

    
}