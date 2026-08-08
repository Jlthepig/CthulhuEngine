
#include "components.h"
#include "flecs.h"
#include "sceneLoader.h"
#include "fwd.hpp"
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
#include "audio.h"
#include "renderer.h"
#include "scene.h"
#include "systemRegistry.h"
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
        else
        {
            Log::Print("GLFW INITIALIZED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
        else
        {
            Log::Print("WINDOW CREATED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
        }

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            Log::Print("FAILED TO INITIALISE GLAD.", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }
        else
        {
            Log::Print("GLAD INITIALISED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
        }

        Cthulhu::Physics::PhysicsConfig physicsConfig;
        physicsWorld.init(physicsConfig);
        physicsWorld.createGroundPlane();
        scene = std::make_unique<Cthulhu::Scene::Scene>();
        camera = Scene::Camera::init();
        Core::Input::init(glfwWindow, resolution);
        Core::Audio::init();
        Cthulhu::Rendering::RenderConfig renderConfig;
        renderer.init(glfwWindow, camera, renderConfig);

        int fbW, fbH;
        glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        Scene::RegisterCoreSystems(scene->getWorld(), this);

        physicsWorld.onFixedUpdate = physicsFixedUpdateCallback;
        physicsWorld.onFixedUpdateContext = this;
    }

    void Engine::loadScene(const std::string &path)
    {
        Scene::SceneLoader::load(path, *scene, physicsWorld);

        renderer.setDirectionalLight(scene->getDirectionalLight());
        for (const auto& light : scene->getPointLights())
        {
            renderer.addPointLight(light);
        }
    
        renderer.setScene(scene.get());
    }

    void Engine::processFixedUpdate(float fixedDt)
    {
            scene->getWorld().each([fixedDt, this]([[maybe_unused]] flecs::entity e, Scene::CharacterControllerComponent& cc) {
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

    void Engine::applySimStateToSystems()
    {
        if (!scene) return;
        bool gameActive = (simState == SimulationState::Running || simState == SimulationState::Stepping);
        auto& world = scene->getWorld();

        // game systems disabled in editor
        auto toggle = [&](const char* name)
        {
            flecs::entity system = world.lookup(name);
            if (system)
            {
                if (gameActive) system.enable();
                else system.disable();
            }
        };

        toggle("PhysicsSyncSystem");
        toggle("CharacterInterpolationSystem");
        toggle("WeaponSystem");
        toggle("AudioSystem");
    }

    void Engine::setSimulationState(SimulationState state)
    {
        simState = state;
        applySimStateToSystems();
    }   

    void Engine::stepSimulation()
    {
        simState = SimulationState::Stepping;
        applySimStateToSystems();
    }

    void Engine::run()
    {
        lastFrame = (float)glfwGetTime();
        while (!glfwWindowShouldClose(glfwWindow))
        {
                double currentFrame = (float)glfwGetTime();
                deltaTime = (float) (currentFrame - lastFrame);
                lastFrame = currentFrame;
                fpsTimer += deltaTime;
                frameCount++;

                if (fpsTimer >= 1.0f) 
                {
                    displayFPS = (float)frameCount / fpsTimer;
                    frameCount = 0;
                    fpsTimer = 0.0f;
                }
                Core::Input::update();    
                Core::Audio::update();

                if (simState == SimulationState::Running || simState == SimulationState::Stepping)
                {
                    physicsWorld.step(deltaTime);
                }

                // ecs systems are always progressing
                // the game systems are automatically handled by applySimStateToSystems()
                scene->getWorld().progress(deltaTime);

                if (simState == SimulationState::Stepping)
                {
                    simState = SimulationState::Paused;
                    applySimStateToSystems();
                }

                if (updateCallback) 
                {
                    updateCallback(updateContext, deltaTime);
                }

                int fbw, fbh;
                glfwGetFramebufferSize(glfwWindow, &fbw, &fbh);

                frameRenderables.clear();
                if (fbw > 0 && fbh > 0 && scene)
                {
                   
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

                renderer.render(fbw, fbh, displayFPS, deltaTime, frameRenderables);
                }

                glfwSwapBuffers(glfwWindow);
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
        Core::Audio::shutdown();
        physicsWorld.shutdown();
        renderer.shutdown();
        scene->clear();
        glfwTerminate();
    }
}