#include <vector>
#include <cmath>
#include <cstddef>

#include <common.hpp>
#include <ext/matrix_transform.hpp>
#include <geometric.hpp>
#include <gtc/random.hpp>
#include <trigonometric.hpp>

#include <GLFW/glfw3.h>
#include <glad.h>

#include "camera.hpp"
#include "components.hpp"
#include "scene.hpp"
#include "input.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scene
{
static std::vector<unique_ptr<Camera>> cameraContainer;

std::string_view gameCameraMessage(GameCameraStatus status)
{
    switch (status)
    {
        case GameCameraStatus::Missing:
            return "No active Camera. Showing the preview view.";
        case GameCameraStatus::NoPrimary:
            return "Several Cameras: mark one Primary. Showing the preview view.";
        case GameCameraStatus::MultiplePrimary:
            return "Several Cameras are Primary: keep one. Showing the preview view.";
        case GameCameraStatus::Invalid:
            return "Invalid Camera position, direction or FOV. Showing the preview view.";
        case GameCameraStatus::Editor:
        case GameCameraStatus::Ready:
            return {};
    }

    return {};
}

GameCameraStatus Camera::updateFromScene(Scene &scene)
{
    std::size_t cameraCount{};
    std::size_t primaryCount{};
    flecs::entity soleCamera;
    flecs::entity primaryCamera;

    scene.getWorld().each(
        [&](flecs::entity entity, const CameraComponent &component) {
            if (!entity.has<TagActive>())
                return;

            ++cameraCount;
            soleCamera = entity;

            if (component.primary)
            {
                ++primaryCount;
                primaryCamera = entity;
            }
        });

    if (cameraCount == 0)
        return GameCameraStatus::Missing;

    if (primaryCount > 1)
        return GameCameraStatus::MultiplePrimary;

    if (primaryCount == 0 && cameraCount > 1)
        return GameCameraStatus::NoPrimary;

    const auto selected = primaryCount == 1 ? primaryCamera : soleCamera;
    const auto *transform = selected.try_get<TransformComponent>();
    const auto *component = selected.try_get<CameraComponent>();

    if (!transform || !component)
        return GameCameraStatus::Invalid;

    const auto finite = [](const glm::vec3 &value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };

    const glm::vec3 worldPosition{transform->cachedModelMatrix[3]};
    const float lengthSquared = glm::dot(component->front, component->front);

    if (!finite(worldPosition) || !finite(component->front) || !std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12f ||
        !std::isfinite(component->fov) || component->fov < 1.0f || component->fov > 179.0f)
    {
        return GameCameraStatus::Invalid;
    }

    position = worldPosition;
    front = component->front / std::sqrt(lengthSquared);
    fov = component->fov;
    up = glm::vec3(0.0f, 1.0f, 0.0f);
    yaw = glm::degrees(std::atan2(front.z, front.x));
    pitch = glm::degrees(std::asin(glm::clamp(front.y, -1.0f, 1.0f)));
    currentRecoil = glm::vec2(0.0f);

    return GameCameraStatus::Ready;
}

Camera *Camera::init()
{
    unique_ptr<Camera> camera = std::make_unique<Camera>();
    Camera *camera_ptr = camera.get();
    camera_ptr->position = glm::vec3(0.0f, 0.0f, 3.0f);
    camera_ptr->front = glm::vec3(0.0f, 0.0f, -1.0f);
    camera_ptr->up = glm::vec3(0.0f, 1.0f, 0.0f);

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

void Camera::processMouse(float xoffset, float yoffset)
{
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    updateFrontVector();
}

void Camera::addRecoil(float pitchKick, float yawSpread)
{
    currentRecoil.y += pitchKick; // kick cam up
    currentRecoil.x += glm::linearRand(-yawSpread, yawSpread); // horizontal sway
    currentRecoil.y = glm::min(currentRecoil.y,
                               15.0f); // caps recoil max 15 degrees upwards kick
}

void Camera::updateRecoil(float deltaTime)
{
    float recoveryFactor = glm::exp(-8.0f * deltaTime); // higher = snappier recovery lower = slower or heavier
    // recovery pistol = higher sniper rifle = lower
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

glm::vec3 Camera::setPosition(const glm::vec3 &newPos)
{
    position = newPos;
    return position;
}

void Camera::addSpeed(float additionalSpeed)
{
    speed += additionalSpeed;
    speed = glm::clamp(speed, 1.0f, 25.0f);
}

glm::mat4 Camera::getViewMatrix() const
{
    glm::vec3 finalFront = front;
    const glm::vec3 worldUp = std::abs(front.y) > 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(front, worldUp));
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    if (currentRecoil != glm::vec2(0.0f))
    {
        finalFront = glm::normalize(glm::rotate(glm::mat4(1.0f), glm::radians(currentRecoil.y), right) * glm::vec4(finalFront, 0.0f));
        finalFront = glm::normalize(glm::rotate(glm::mat4(1.0f), glm::radians(currentRecoil.x), up) * glm::vec4(finalFront, 0.0f));
    }
    return glm::lookAt(position, position + finalFront, up);
}

glm::vec3 Camera::getFront() const
{
  return front;
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

} // namespace Cthulhu::Scene
