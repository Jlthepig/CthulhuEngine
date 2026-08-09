#pragma once


#include "fwd.hpp"
#include "glm.hpp"

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
        void processKeyboard(double deltaTime);
        void processMouse(float xoffset,float yoffset);
        glm::vec3 getPosition() const;
        glm::vec3 setPosition(const glm::vec3& newPos);
        void addSpeed(float additionalSpeed) { speed += additionalSpeed; speed = glm::clamp(speed, 1.0f, 25.0f);}
        glm::mat4 getViewMatrix() const;
        glm::vec3 getFront() const { return front; }
        float getFov() const;
        
        void addRecoil(float pitchKick, float yawSpread);
        void updateRecoil(float deltaTime);

        private:
        float yaw;
        float pitch;
        float speed;
        float sensitivity;
        float fov;
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;

        glm::vec2 currentRecoil = glm::vec2(0.0f);
        void updateFrontVector();
    };
}