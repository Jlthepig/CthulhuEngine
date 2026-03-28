#pragma once


#include "glm.hpp"
#include "glfw3.h"
#include <algorithm>
namespace Cthulhu::Scene
{
    class Camera
    {
        public:
        void init();
        void processKeyboard(GLFWwindow *window, double Time);
        void processMouse(float xoffset,float yoffset);
        glm::mat4 getViewMatrix() const;
        float getFov() const;
        

        private:
        bool firstMouse = true;
        float yaw;
        float pitch;
        float speed;
        float sensitivity;
        float fov;
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;

        void updateFrontVector();
    };
}