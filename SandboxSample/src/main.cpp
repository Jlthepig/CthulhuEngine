#include "engine.h"
#include "characterController.h"
#include "camera.h"
#include "input.h"
#include "glfw3.h"
#include "log_utils.hpp"

namespace GameConfig
{
    constexpr glm::vec2 WINDOW_RESOLUTION = glm::vec2(1920.0f, 1080.0f);
    constexpr const char* WINDOW_TITLE = "Cthulhu Engine";
    constexpr const char* SCENE_PATH = "assets/scenes/test.scene";
    constexpr glm::vec3 CHARACTER_START_POSITION = glm::vec3(0, 2, 0);
    constexpr float CAMERA_EYE_HEIGHT_OFFSET = 0.6f;
    constexpr glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
}

static bool inEditorMode = true;
static Cthulhu::Scene::Camera* camera = nullptr;

void onUpdate(float deltaTime)
{
    if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_F1))
    {
        inEditorMode = !inEditorMode;
        KalaHeaders::KalaLog::Log::Print(inEditorMode ? "Switched to Editor Mode" : "Switched to Game Mode", "Game", KalaHeaders::KalaLog::LogType::LOG_INFO);
    }

    if (inEditorMode)
    {
        camera->processKeyboard(deltaTime);
    }
    else
    {
        glm::vec3 movement(0.0f);
        glm::vec3 front = camera->getFront();
        front.y = 0.0f;
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, GameConfig::WORLD_UP));

        if (Cthulhu::Core::Input::isKeyDown(GLFW_KEY_W)) movement += front;
        if (Cthulhu::Core::Input::isKeyDown(GLFW_KEY_S)) movement -= front;
        if (Cthulhu::Core::Input::isKeyDown(GLFW_KEY_A)) movement -= right;
        if (Cthulhu::Core::Input::isKeyDown(GLFW_KEY_D)) movement += right;

        if (glm::length(movement) > 0.0f)
            movement = glm::normalize(movement);

        bool jump = Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_SPACE);

        Cthulhu::Physics::CharacterController::update(movement, jump, deltaTime);
        glm::vec3 charPos = Cthulhu::Physics::CharacterController::getPosition();
        charPos.y += GameConfig::CAMERA_EYE_HEIGHT_OFFSET;
        camera->setPosition(charPos);
    }
}

int main()
{
    Cthulhu::Engine::init(GameConfig::WINDOW_TITLE, GameConfig::WINDOW_RESOLUTION);
    Cthulhu::Engine::loadScene(GameConfig::SCENE_PATH);

    Cthulhu::Physics::CharacterController::init(glm::vec3(0, GameConfig::CHARACTER_START_POSITION.y, 0));

    camera = Cthulhu::Engine::getCamera();

    Cthulhu::Engine::setUpdateCallback(onUpdate);
    Cthulhu::Engine::run();
    Cthulhu::Physics::CharacterController::destroy();
    Cthulhu::Engine::shutdown();
    return 0;
}