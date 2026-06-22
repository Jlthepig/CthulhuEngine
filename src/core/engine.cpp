#include "ext/quaternion_geometric.hpp"
#include "fwd.hpp"
#include "pch.h"
#include "components.h"
#include "sceneLoader.h"
#include <cstdlib>

 #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad.h"
#include "glfw3.h"
#include "log_utils.hpp"

#include "Jolt/Jolt.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "Jolt/Core/TempAllocator.h"

#include "engine.h"
#include "camera.h"
#include "window.h"
#include "input.h"
#include "renderer.h"
#include "scene.h"
#include "sceneLoader.h"
#include "physics.h"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// Static callback to bridge C-style function pointer to Engine class
static void physicsFixedUpdateCallback(void* context, float fixedDt) {
    static_cast<Cthulhu::Engine*>(context)->processFixedUpdate(fixedDt);
}
namespace Cthulhu
{
    void Engine::init(const char* title, glm::vec2 resolution)
    {
        if (!glfwInit()) 
        {
            Log::Print("CANNOT INITIALIZE GLFW", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        Cthulhu::Core::WindowConfig windowConfig;
        windowConfig.resolution = resolution;
        window = Cthulhu::Core::Window::createWindow(windowConfig, title);
        
        glfwWindow = window->getWindow();
        if (glfwWindow == NULL)
        {
            Log::Print("WINDOW IS NULL", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            Log::Print("FAILED TO INITIALISE GLAD.", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }

        Cthulhu::Physics::PhysicsConfig physicsConfig;
        physicsWorld.init(physicsConfig);
        physicsWorld.createGroundPlane();
        scene = std::make_unique<Cthulhu::Scene::Scene>();
        camera = Scene::Camera::init();
        Core::Input::init(glfwWindow, resolution);

        Cthulhu::Rendering::RenderConfig renderConfig;
        renderer.init(glfwWindow, camera, renderConfig);

        int fbW, fbH;
        glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // register physics sync system runs before transform system
        scene->getWorld().system<Scene::TransformComponent, const Scene::PhysicsComponent>("PhysicsSyncSystem")
            .each([this](Scene::TransformComponent& transform, const Scene::PhysicsComponent& phys)
            {
                if (phys.hasBody)
                {
                    auto bodyTransform = physicsWorld.getBodyTransform(phys.bodyId);
                    transform.position = bodyTransform.position;
                    transform.rotation = bodyTransform.rotation;
                    transform.matrixDirty = true;
                }
            }
        );
        // register transform system
        scene->getWorld().system<Cthulhu::Scene::TransformComponent>("TransformSystem")
            .each([](flecs::entity e, Cthulhu::Scene::TransformComponent& transform)
            {

                glm::mat4 localMatrix = glm::mat4(1.0f);
                localMatrix = glm::translate(localMatrix, transform.position);
                localMatrix = glm::rotate(localMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                localMatrix = glm::rotate(localMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                localMatrix = glm::rotate(localMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                localMatrix = glm::scale(localMatrix, transform.scale);

                // combine with parent matrix if it has a parent
                glm::mat4 globalMatrix = localMatrix;
                auto parent = e.parent();
                if (parent.is_alive() && parent.has<Scene::TransformComponent>())
                {
                    const auto& parentTransform = parent.get<Scene::TransformComponent>();
                    globalMatrix = parentTransform.cachedModelMatrix * localMatrix;
                }
                transform.cachedModelMatrix = globalMatrix;
                transform.cachedNormalMatrix = glm::transpose(glm::inverse(globalMatrix));
            });
            
            physicsWorld.onFixedUpdate = physicsFixedUpdateCallback;
            physicsWorld.onFixedUpdateContext = this;

        scene->getWorld().system<Scene::CharacterControllerComponent, Scene::TransformComponent>("CharacterInterpolationSystem")
            .each([this](Scene::CharacterControllerComponent& cc, Scene::TransformComponent& transform) 
            {
                float alpha = physicsWorld.getInterpolationAlpha();
                transform.position = glm::mix(cc.prevPos, cc.currentPos, alpha);
                transform.matrixDirty = true;
            });
        
        scene->getWorld().system<Scene::WeaponComponent, const Scene::TransformComponent>("WeaponSystem")
            .each([this](flecs::entity e, Scene::WeaponComponent& wep, const Scene::TransformComponent& trans)
            {
                // cooldown
                wep.timeSinceLastShot += deltaTime;

                if (wep.wantsToFire)
                {
                    float cooldown = 1.0 / wep.firerate;

                    if (wep.timeSinceLastShot >= cooldown)
                    {
                        glm::vec3 origin = glm::vec3(trans.cachedModelMatrix[3]);
                        glm::vec3 forward = -glm::vec3(trans.cachedModelMatrix[2]);
                        forward = glm::normalize(forward);

                        Physics::RaycastHitInfo hit = physicsWorld.raycast(origin, forward, wep.maxRange);

                        if (!hit.didHit)
                        {
                            hit.position = origin + (forward * wep.maxRange);
                            hit.distance = wep.maxRange;
                        }

                        if (raycastCallback)
                        {
                            raycastCallback(raycastContext,hit);
                        }
                        wep.timeSinceLastShot = 0.0f;
                    }

                    wep.wantsToFire = false;
                }
            });
    }

    void Engine::loadScene(const std::string &path)
    {
        sceneData = Scene::SceneLoader::load(path, *scene, physicsWorld);

        renderer.setDirectionalLight(sceneData.directionalLight);
        for (const auto& light : sceneData.pointLights)
        {
            renderer.addPointLight(light);
        }
    
        renderer.setScene(scene.get());
    }

    void Engine::processFixedUpdate(float fixedDt)
    {
            scene->getWorld().each([fixedDt, this](flecs::entity e, Scene::CharacterControllerComponent& cc) {
            if (!cc.character) return;

            JPH::RVec3 joltPos = cc.character->GetPosition();
            cc.prevPos = glm::vec3(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());

            bool isOnGround = cc.character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
            float gravity = -9.81f; 
            float jumpVel = 5.0f;

            if (isOnGround) {
                cc.verticalVelocity = 0.0f;
                if (cc.pendingJump) cc.verticalVelocity = jumpVel;
            } else {
                cc.verticalVelocity += gravity * fixedDt;
            }

            JPH::Vec3 velocity(cc.pendingMove.x, cc.verticalVelocity, cc.pendingMove.z);
            cc.character->SetLinearVelocity(velocity);

            JPH::CharacterVirtual::ExtendedUpdateSettings s;
            cc.character->ExtendedUpdate(
                fixedDt, JPH::Vec3(0, gravity, 0), s,
                physicsWorld.getPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(1), // MOVING
                physicsWorld.getPhysicsSystem()->GetDefaultLayerFilter(1),
                {}, {}, *physicsWorld.getTempAllocator()
            );

            joltPos = cc.character->GetPosition();
            cc.currentPos = glm::vec3(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());

            cc.pendingJump = false;
        });
    }

    void Engine::run()
    {
       while (!glfwWindowShouldClose(glfwWindow))
       {
            Core::Input::update();    
        
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            physicsWorld.step(deltaTime);

            scene->getWorld().progress(deltaTime);

            if (updateCallback) 
            {
                updateCallback(updateContext,deltaTime);
            }

            frameRenderables.clear();
            scene->getWorld().each([&](flecs::entity e, const Scene::TransformComponent& transform, const Scene::MeshComponent& mesh) {
                if (e.has<Scene::TagActive>() && mesh.model) {
                    frameRenderables.push_back({
                        mesh.model,
                        transform.cachedModelMatrix,
                        transform.cachedNormalMatrix,
                        mesh.boundsMin,
                        mesh.boundsMax
                    });
                }
            });
            renderer.render(deltaTime, frameRenderables);
            glfwPollEvents();
       }
    }

    void Engine::setUpdateCallback(UpdateCallback callback, void* context)
    {
        updateCallback = callback;
        updateContext = context;
    }

    void Engine::setRaycastCallback(RaycastCallback callback, void* context)
    {
        raycastCallback = callback;
        raycastContext = context;
    }

    Scene::Camera* Engine::getCamera() { return camera; }

    void Engine::shutdown()
    {
        physicsWorld.shutdown();
        renderer.shutdown();
        scene->clear();
        glfwTerminate();
    }
}