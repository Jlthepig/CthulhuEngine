#pragma once


#include "glad.h"
#include "glfw3.h"
#include "glm.hpp"
#include <string>
#include <memory>
#include <vector>

using std::unique_ptr;
using std::make_unique;
namespace Cthulhu::Scene {class Camera;}
namespace Cthulhu::Core 
{
    class Window
    {
        public:
        

        static Window* createWindow(glm::vec2 windowSize,const char* windowTitle);
        void setCamera(Cthulhu::Scene::Camera* cam);
        
        GLFWwindow* getWindow() const;
        float getWidth();
        float getHeight();
        static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
        static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
        static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

        private:
        GLFWwindow* glfWwindow = nullptr;
        Cthulhu::Scene::Camera* camera = nullptr;
        glm::vec2 windowSize = glm::vec2(1920.0f,1080.0f);
        bool firstMouse = true;
        float lastX = windowSize.x/2.0f;
        float lastY = windowSize.y/2.0f;
    };
}
