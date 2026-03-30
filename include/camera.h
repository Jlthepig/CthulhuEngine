#pragma once


#include "glm.hpp"
#include "glfw3.h"
#include <algorithm>
#include <memory>

using std::unique_ptr;
using std::make_unique;

namespace Cthulhu::Scene
{
    class Camera
    {
        public:
        static Camera* init();
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