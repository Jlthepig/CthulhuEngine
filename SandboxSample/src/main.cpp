#include "components.h"
#include "engine.h"
#include "characterController.h"
#include "camera.h"
#include "fwd.hpp"
#include "input.h"
#include "glfw3.h"
#include "log_utils.hpp"
#include "physics.h"
namespace GameConfig
{
    constexpr glm::vec2 WINDOW_RESOLUTION = glm::vec2(1920.0f, 1080.0f);
    constexpr const char* WINDOW_TITLE = "Cthulhu Engine";
    constexpr const char* SCENE_PATH = "assets/scenes/test.scene";
    constexpr glm::vec3 CHARACTER_START_POSITION = glm::vec3(0, 4, 0);
    constexpr float CAMERA_EYE_HEIGHT_OFFSET = 0.6f;
    constexpr glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
}

static bool inEditorMode = true;
static Cthulhu::Scene::Camera* camera = nullptr;
static Cthulhu::Engine* enginePtr = nullptr;
static flecs::entity playerEntity;
static flecs::entity cameraEntity;

void onUpdate(float deltaTime)
{   
    if (camera) {
        camera->processMouse(Cthulhu::Core::Input::getMouseDeltaX(), Cthulhu::Core::Input::getMouseDeltaY());
    }

    if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_F1))
    {
        inEditorMode = !inEditorMode;
        KalaHeaders::KalaLog::Log::Print(inEditorMode ? "Switched to Editor Mode" : "Switched to Game Mode", "Game", KalaHeaders::KalaLog::LogType::LOG_INFO);

        if (!inEditorMode)
        {
            glm::vec3 camPos = camera->getPosition();
            glm:: vec3 feetPos = camPos - glm::vec3(0, GameConfig::CAMERA_EYE_HEIGHT_OFFSET, 0);
            Cthulhu::Physics::CharacterController::setPosition(feetPos);
            Cthulhu::Physics::CharacterController::resetVerticalVelocity();

            if (playerEntity.is_alive())
            {
                auto& transform = playerEntity.get_mut<Cthulhu::Scene::TransformComponent>();
                transform.position = feetPos;
                transform.matrixDirty = true;
            }
        }
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

        Cthulhu::Physics::CharacterController::queueInput(movement, jump);

        float alpha = enginePtr->getPhysicsWorld().getInterpolationAlpha();
        glm::vec3 charPos = Cthulhu::Physics::CharacterController::getInterpolationPosition(alpha);

        
        if (playerEntity.is_alive())
        {
            auto& transform = playerEntity.get_mut<Cthulhu::Scene::TransformComponent>();
            transform.position = charPos;
        }

        if (cameraEntity.is_alive())
        {
            const auto* camTransform = cameraEntity.try_get<Cthulhu::Scene::TransformComponent>();
            if (camTransform)
            {
                glm::vec3 globalCamPos = glm::vec3(camTransform->cachedModelMatrix[3]);
                camera->setPosition(globalCamPos);
            }
        }

    }
}

int main()
{
    // Instantiate the Engine
    Cthulhu::Engine engine;
    enginePtr = &engine;
    
    engine.init(GameConfig::WINDOW_TITLE, GameConfig::WINDOW_RESOLUTION);
    engine.loadScene(GameConfig::SCENE_PATH);
    
    playerEntity = engine.getScene().getWorld().entity("Player");
    playerEntity.set<Cthulhu::Scene::TransformComponent>({glm::vec3(0,GameConfig::CHARACTER_START_POSITION.y,0)});
    playerEntity.add<Cthulhu::Scene::TagPlayer>();
    playerEntity.add<Cthulhu::Scene::TagActive>();

    cameraEntity = engine.getScene().getWorld().entity("Camera");
    cameraEntity.child_of(playerEntity);
    cameraEntity.set<Cthulhu::Scene::TransformComponent>({glm::vec3(0,GameConfig::CAMERA_EYE_HEIGHT_OFFSET,0)});
    cameraEntity.add<Cthulhu::Scene::TagActive>();

    Cthulhu::Physics::CharacterConfig charConfig;
    Cthulhu::Physics::CharacterController::init(glm::vec3(0, GameConfig::CHARACTER_START_POSITION.y, 0), charConfig, engine.getPhysicsWorld());
    
    camera = engine.getCamera();
    engine.setUpdateCallback(onUpdate);
    engine.run();
    
    Cthulhu::Physics::CharacterController::destroy();
    engine.shutdown();
    return 0;
}