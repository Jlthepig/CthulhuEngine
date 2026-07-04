#include "common.hpp"
#include "fwd.hpp"
#include "geometric.hpp"
#include "camera.h"
#include "glad.h"        
#include "glfw3.h"
#include "input.h"
#include "ext/matrix_transform.hpp"
#include "gtc/random.hpp"
#include "log_utils.hpp"
#include "trigonometric.hpp"
#include <vector>
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
    static std::vector<unique_ptr<Camera>> cameraContainer;
    Camera* Camera::init()
    {
        unique_ptr<Camera> camera = std::make_unique<Camera>();
        Camera* camera_ptr = camera.get();
        camera_ptr->position = glm::vec3(0.0f,0.0f,3.0f);
        camera_ptr->front = glm::vec3(0.0f,0.0f,-1.0f);
        camera_ptr->up= glm::vec3(0.0f,1.0f,0.0f);

        camera_ptr->yaw = -90.0f;
        camera_ptr->pitch = 0.0f;
        camera_ptr->speed = 2.5f;
        camera_ptr->sensitivity = 0.1f;
        camera_ptr->fov = 90.0f;
        camera_ptr->updateFrontVector();

        cameraContainer.push_back(std::move(camera));
        return camera_ptr;
        Log::Print("CAMERA INITIALIZED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
    }


    void Camera::updateFrontVector()
    {
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        this->front = glm::normalize(front);
    }

    void Camera::processMouse(float xoffset,float yoffset)
    {
        
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;
        pitch = std::clamp(pitch,-89.0f,89.0f);
        updateFrontVector();
        
    }

    void Camera::addRecoil(float pitchKick, float yawSpread)
    {
        currentRecoil.y += pitchKick; // kick cam up
        currentRecoil.x += glm::linearRand(-yawSpread,yawSpread); // horizontal sway
        currentRecoil.y = glm::min(currentRecoil.y,15.0f); // caps recoil max 15 degrees upwards kick
    }

    void Camera::updateRecoil(float deltaTime)
    {
        float recoveryFactor = glm::exp(-8.0f * deltaTime); // higher = snappier recovery lower = slower or heavier recovery pistol = higher sniper rifle = lower
        currentRecoil *= recoveryFactor;
        
        if (glm::abs(currentRecoil.x) < 0.001f && glm::abs(currentRecoil.y) < 0.001f)
        {
            currentRecoil = glm::vec2(0.0f);
        }
    }
    
    glm::vec3 Camera::getPosition() const
    {
        return position;
    }

    glm::vec3 Camera::setPosition(const glm::vec3& newPos)
    {
        position = newPos;
        return position;
    }
    
    glm::mat4 Camera::getViewMatrix() const
    {
        glm::vec3 finalFront = front;
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f); 
        glm::vec3 right = glm::normalize(glm::cross(front, worldUp));
        glm::vec3 up = glm::normalize(glm::cross(right, front));

        if (currentRecoil != glm::vec2(0.0f))
        {
            finalFront = glm::normalize(glm::rotate(glm::mat4(1.0f), glm::radians(currentRecoil.y), right) * glm::vec4(finalFront,0.0f));
            finalFront = glm::normalize(glm::rotate(glm::mat4(1.0f), glm::radians(currentRecoil.x), up) * glm::vec4(finalFront, 0.0f));
        }
        return glm::lookAt(position, position + finalFront,up);
    }
    
    float Camera::getFov() const
    {
        return fov;
    }

    void Camera::processKeyboard(double deltaTime)
    {
        float currentSpeed = speed * deltaTime;

        glm::vec3 right = glm::normalize(glm::cross(front, up));

        if (Core::Input::isKeyPressed(GLFW_KEY_ESCAPE))
            glfwSetInputMode(Core::Input::getWindowHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (Core::Input::isKeyDown(GLFW_KEY_W))
            position += currentSpeed * front;
        if (Core::Input::isKeyDown(GLFW_KEY_S))
            position -= currentSpeed * front;
        if (Core::Input::isKeyDown(GLFW_KEY_A))
            position -= right * currentSpeed;
        if (Core::Input::isKeyDown(GLFW_KEY_D))
            position += right * currentSpeed;
        if (Core::Input::isKeyDown(GLFW_KEY_Q))
            position += currentSpeed * up;
        if (Core::Input::isKeyDown(GLFW_KEY_E))
            position -= currentSpeed * up;
    }
    
   
    
    
}