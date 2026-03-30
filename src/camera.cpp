
#include "camera.h"
#include "ext/matrix_transform.hpp"
#include <string>
#include <vector>
#include "log_utils.hpp"

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
        Log::Print(std::to_string(xoffset) + ", " + std::to_string(yoffset));
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;
        pitch = std::clamp(pitch,-89.0f,89.0f);
        updateFrontVector();
        
    }
    
    glm::mat4 Camera::getViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }
    
    float Camera::getFov() const
    {
        return fov;
    }

    void Camera::processKeyboard(GLFWwindow *window, double deltaTime)
    {
        float currentSpeed = speed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += currentSpeed * front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= currentSpeed * front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= glm::normalize(glm::cross(front, up)) * currentSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += glm::normalize(glm::cross(front, up)) * currentSpeed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            position += currentSpeed * up;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            position -= currentSpeed * up;
    }
    
   
    
    
}