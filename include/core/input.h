#pragma once

#include "fwd.hpp"
#include "glad.h"
#include "glfw3.h"
#include "glm.hpp"

namespace Cthulhu::Scene {class Camera;}

namespace Cthulhu::Core
{
    class Input
    {
        public:
        static void init(GLFWwindow* window, glm::vec2 resolution);
        static void setCamera(Cthulhu::Scene::Camera* cam);
        static GLFWwindow* getWindowHandle();
        static void update();
        
        static bool isKeyDown(int key);
        static bool isKeyPressed(int key);
        static bool isKeyReleased(int key);

        static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
        static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

        static float getMouseX();
        static float getMouseY();
        static float getMouseDeltaX();
        static float getMouseDeltaY();

        private:
        static GLFWwindow* windowHandle;
        static Cthulhu::Scene::Camera* camera;
        static bool currentKeys[GLFW_KEY_LAST + 1];
        static bool previousKeys[GLFW_KEY_LAST + 1];
        static bool firstMouse;
        static float mouseDeltaX;
        static float mouseDeltaY;
        static float lastX;
        static float lastY;
    };
}