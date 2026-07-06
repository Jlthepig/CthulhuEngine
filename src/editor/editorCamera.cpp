#include "editorCamera.h"
#include "common.hpp"
#include "input.h"
#include "glfw3.h"
#include "trigonometric.hpp"

namespace Cthulhu::Editor
{
    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearPlane, float farPlane)
        : fov(fov), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane) 
    {
        updateCameraVectors();
    }

    void EditorCamera::update([[maybe_unused]]float deltaTime)
    {
        if (Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
        {
            float deltaX = Core::Input::getMouseDeltaX() * orbitSpeed;
            float deltaY = Core::Input::getMouseDeltaY() * orbitSpeed;

            yaw += deltaX;
            pitch += deltaY;

            // clamp prevent upside down
            pitch = glm::clamp(pitch,glm::radians(-89.0f), glm::radians(89.0f));
        }

        if (Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE))
        {
            float deltaX = Core::Input::getMouseDeltaX() * panSpeed;
            float deltaY = Core::Input::getMouseDeltaY() * panSpeed;

            float panScale = distance * 0.1f; // scale by distance

            focalPoint -= rightVector * deltaX * panScale;
            focalPoint += upVector * deltaY * panScale;
        }

        float scrollDelta = Core::Input::getScrollDelta();
        if (scrollDelta != 0.0f)
        {
            distance -= scrollDelta * zoomSpeed * (distance * 0.1f);
            distance = glm::clamp(distance,minDistance,maxDistance);
        }

        updateCameraVectors();
    }

    void EditorCamera::updateCameraVectors()
    {
        position.x = focalPoint.x + distance * cos(pitch) * sin(yaw);
        position.y = focalPoint.y + distance * sin(pitch);
        position.z = focalPoint.z + distance * cos(pitch) * cos(yaw);

        frontVector = glm::normalize(focalPoint - position);
        rightVector = glm::normalize(glm::cross(frontVector, glm::vec3(0.0f, 1.0f, 0.0f)));
        upVector = glm::cross(rightVector, frontVector);

        viewMatrix = glm::lookAt(position, focalPoint, glm::vec3(0.0f, 1.0f, 0.0f));
        projectionMatrix = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
    }

    const glm::mat4& EditorCamera::getViewMatrix() const {return viewMatrix;}
    const glm::mat4& EditorCamera::getProjectionMatrix() const { return projectionMatrix; }
    glm::vec3 EditorCamera::getPosition() const { return position; }
    glm::vec3 EditorCamera::getFocalPoint() const { return focalPoint; }

    void EditorCamera::setFocalPoint(const glm::vec3& point)
    {
        focalPoint = point;
        updateCameraVectors();
    }

    void EditorCamera::setAspectRatio(float ratio)
    {
        aspectRatio = ratio;
        updateCameraVectors();
    }
}