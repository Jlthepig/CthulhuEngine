#pragma once

#include <memory>
#include <string_view>

#include <glm.hpp>

using std::make_unique;
using std::unique_ptr;

namespace Cthulhu::Scene
{

class Scene;

enum class GameCameraStatus
{
    Editor,
    Ready,
    Missing,
    NoPrimary,
    MultiplePrimary,
    Invalid
};

std::string_view gameCameraMessage(GameCameraStatus status);
class Camera
{
  public:
    static Camera *init();
    void processKeyboard(double deltaTime);
    void processMouse(float xoffset, float yoffset);
    glm::vec3 getPosition() const;
    glm::vec3 setPosition(const glm::vec3 &newPos);

    [[nodiscard]] GameCameraStatus updateFromScene(Scene &scene);

    void addSpeed(float additionalSpeed);

    glm::mat4 getViewMatrix() const;

    glm::vec3 getFront() const;

    float getFov() const;

    void addRecoil(float pitchKick, float yawSpread);
    void updateRecoil(float deltaTime);

  private:
    float yaw{};
    float pitch{};
    float speed{};
    float sensitivity{};
    float fov{};
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    glm::vec2 currentRecoil = glm::vec2(0.0f);
    void updateFrontVector();
};
} // namespace Cthulhu::Scene
