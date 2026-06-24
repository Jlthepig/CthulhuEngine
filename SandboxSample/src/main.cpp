#include "components.h"
#include "engine.h"
#include "characterController.h"
#include "camera.h"
#include "fwd.hpp"
#include "input.h"
#include "glfw3.h"
#include "log_utils.hpp"
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
static flecs::entity playerEntity;
static flecs::entity cameraEntity;

void onUpdate(void* context, float deltaTime)
{   
    if (camera) {
        camera->processMouse(Cthulhu::Core::Input::getMouseDeltaX(), Cthulhu::Core::Input::getMouseDeltaY());
        camera->updateRecoil(deltaTime);
    }

    if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_F1))
    {
        inEditorMode = !inEditorMode;
        KalaHeaders::KalaLog::Log::Print(inEditorMode ? "Switched to Editor Mode" : "Switched to Game Mode", "Game", KalaHeaders::KalaLog::LogType::LOG_INFO);

        if (!inEditorMode)
        {
            glm::vec3 camPos = camera->getPosition();
            glm:: vec3 feetPos = camPos - glm::vec3(0, GameConfig::CAMERA_EYE_HEIGHT_OFFSET, 0);
            
            Cthulhu::Physics::CharacterController::teleport(playerEntity, feetPos);

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

        
        if (playerEntity.has<Cthulhu::Scene::CharacterControllerComponent>())
        {
            auto& cc = playerEntity.ensure<Cthulhu::Scene::CharacterControllerComponent>();
            cc.pendingMove = movement;
            if (jump) cc.pendingJump = true;
        }

        if (cameraEntity.is_alive()) {
            const auto* camTrans = cameraEntity.try_get<Cthulhu::Scene::TransformComponent>();
            if (camTrans) camera->setPosition(glm::vec3(camTrans->cachedModelMatrix[3]));

            auto& camComp = cameraEntity.ensure<Cthulhu::Scene::CameraComponent>();
            camComp.front = camera->getFront();
        }   
        
        bool mouseDown = Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
        if (mouseDown)
        {
            auto& wep = cameraEntity.ensure<Cthulhu::Scene::WeaponComponent>();
            wep.wantsToFire = true;
        }
    }
}

static void onWeaponRaycast(void* context, const Cthulhu::Physics::RaycastHitInfo& hit)
{
    Cthulhu::Engine* engine = static_cast<Cthulhu::Engine*>(context);
    
    glm::vec3 camPos = engine->getCamera()->getPosition();
    glm::vec3 camFront = engine->getCamera()->getFront(); 
    glm::vec3 safeStart = camPos + (camFront * 0.5f); 
    
    glm::vec3 lineColor = hit.didHit ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);

    engine->getRenderer().addDebugLine(safeStart, hit.position, lineColor,1.0f);
    engine->getCamera()->addRecoil(1.2f, 0.2f);
}

int main()
{
    Cthulhu::Engine engine;
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

    Cthulhu::Scene::WeaponComponent playerWeapon;
    playerWeapon.firerate = 12.0f;
    playerWeapon.maxRange = 500.0f;
    cameraEntity.set(playerWeapon);

    Cthulhu::Physics::CharacterConfig charConfig;
    Cthulhu::Physics::CharacterController::create(playerEntity, GameConfig::CHARACTER_START_POSITION, charConfig, engine.getPhysicsWorld());
    
    camera = engine.getCamera();
    
    engine.setRaycastCallback(onWeaponRaycast, &engine);
    engine.setUpdateCallback(onUpdate);
    engine.run();
    
    Cthulhu::Physics::CharacterController::destroy(playerEntity);
    engine.shutdown();
    return 0;
}