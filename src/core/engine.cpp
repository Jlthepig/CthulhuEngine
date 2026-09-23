#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include <glad.h>
#include <glfw3.h>
#include <stb_image.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "applicationPaths.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "flecs.h"
#include "input.hpp"
#include "physics.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "sceneLoader.hpp"
#include "jsonWriter.hpp"
#include "systemRegistry.hpp"
#include "window.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// Static callback to bridge C-style function pointer to Engine class
static void physicsFixedUpdateCallback(void *context, float fixedDt)
{
    static_cast<Cthulhu::Engine *>(context)->processFixedUpdate(fixedDt);
}
namespace Cthulhu
{

Engine::Engine() = default;

Engine::~Engine()
{
    shutdown();
}

bool Engine::init(const std::filesystem::path &projectFilePath)
{
    if (state != EngineState::Uninitialized)
    {
        Log::Print(" ENGINE IS ALREADY INITIALISED", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto openedProject = Cthulhu::Project::Project::open(projectFilePath);

    if (!openedProject)
    {
        Log::Print("FAILED TO OPEN PROJECT", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    project = std::move(*openedProject);

    const auto &projectConfig = project->getConfig();

    auto executableDirectory = Core::getExecutableDirectory();

    if (!executableDirectory)
    {
        Log::Print("FAILED TO DETERMINE EXECUTABLE DIRECTORY", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    engineResourceRoot = *executableDirectory / "EngineResources";

    if (!std::filesystem::is_directory(engineResourceRoot))
    {
        Log::Print("ENGINE RESOURCE DIRECTORY NOT FOUND: " + engineResourceRoot.string(), "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    glm::vec2 resolution(static_cast<float>(projectConfig.windowWidth), static_cast<float>(projectConfig.windowHeight));

    if (!glfwInit())
    {
        Log::Print("CANNOT INITIALIZE GLFW", "ENGINE", LogType::LOG_ERROR);
        return false;
    }
    else
    {
        Log::Print("GLFW INITIALIZED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
        glfwInitialized = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Cthulhu::Core::WindowConfig windowConfig;
    windowConfig.resolution = resolution;

    window = Cthulhu::Core::Window::createWindow(windowConfig, projectConfig.name.c_str());

    if (!window)
    {
        Log::Print("FAILED TO CREATE WINDOW", "ENGINE", LogType::LOG_ERROR);
        shutdown();
        return false;
    }

    glfwWindow = window->getWindow();
    if (glfwWindow == NULL)
    {
        Log::Print("WINDOW DOES NOT CONTAIN A VALID GLFW WINDOW", "ENGINE", LogType::LOG_ERROR);
        shutdown();
        return false;
    }
    else
    {
        Log::Print("WINDOW CREATED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        Log::Print("FAILED TO INITIALISE GLAD.", "ENGINE", LogType::LOG_ERROR);
        shutdown();
        return false;
    }
    else
    {
        Log::Print("GLAD INITIALISED SUCCESSFULLY", "ENGINE", LogType::LOG_SUCCESS);
    }

    Cthulhu::Physics::PhysicsConfig physicsConfig;
    Cthulhu::Rendering::RenderConfig renderConfig;

    renderConfig.engineResourceRoot = engineResourceRoot;

    physicsWorld.init(physicsConfig);
    physicsInitialized = true;

    activeScene = createSceneInstance();
    camera = Scene::Camera::init();

    Core::Input::init(glfwWindow, resolution);
    Core::Audio::init();
    audioInitialized = true;

    renderer.init(glfwWindow, camera, renderConfig);
    rendererInitialized = true;

    int fbW, fbH;
    glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    physicsWorld.onFixedUpdate = physicsFixedUpdateCallback;
    physicsWorld.onFixedUpdateContext = this;

    state = EngineState::Initialized;

    if (!createEmptyScene())
    {
        shutdown();
        return false;
    }

    Log::Print("ENGINE INITIALIZED FOR PROJECT: " + projectConfig.name, "ENGINE", LogType::LOG_SUCCESS);
    return true;
}

void Engine::shutdown()
{
    if (state == EngineState::ShuttingDown)
    {
        return;
    }
    if (state == EngineState::Initialized && !glfwInitialized)
    {
        return;
    }

    state = EngineState::ShuttingDown;

    updateCallback = nullptr;
    updateContext = nullptr;

    raycastCallback = nullptr;
    raycastContext = nullptr;

    unloadScene();

    if (rendererInitialized)
    {
        renderer.shutdown();
        rendererInitialized = false;
    }

    if (audioInitialized)
    {
        Core::Audio::shutdown();
        audioInitialized = false;
    }

    if (physicsInitialized)
    {
        physicsWorld.shutdown();
        physicsInitialized = false;
    }

    camera = nullptr;
    window = nullptr;
    glfwWindow = nullptr;

    project.reset();

    if (glfwInitialized)
    {
        glfwTerminate();
        Core::Window::destroyAll();
        glfwInitialized = false;
    }

    frameRenderables.clear();

    deltaTime = 0.0f;
    lastFrame = 0.0;

    state = EngineState::Uninitialized;
}

std::unique_ptr<Scene::Scene> Engine::createSceneInstance()
{
    auto newScene = std::make_unique<Scene::Scene>();

    Scene::RegisterCoreSystems(newScene->getWorld(), this);
    return newScene;
}

bool Engine::loadScene(std::string_view resourcePath)
{
    if (!project)
    {
        Log::Print("CANNOT LOAD SCENE WITHOUT AN ACTIVE PROJECT", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto resolvedPath = project->resolveResourcePath(resourcePath);

    if (!resolvedPath)
    {
        return false;
    }

    auto newScene = createSceneInstance();
    if (!Scene::SceneLoader::load(resolvedPath->string(), *newScene, physicsWorld, *project))
    {
        Log::Print("FAILED TO LOAD SCENE: " + std::string(resourcePath), "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    newScene->setResourcePath(std::string(resourcePath));
    newScene->markClean();

    activateScene(std::move(newScene));
    Log::Print("ACTIVE SCENE CHANGED TO: " + activeScene->getName(), "ENGINE", LogType::LOG_SUCCESS);
    return true;
}

bool Engine::createEmptyScene(const std::string &name)
{
    if (!isInitialized())
    {
        Log::Print("CANNOT CREATE SCENE BEFORE ENGINE INITIALIZATION", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto newScene = createSceneInstance();

    newScene->setName(name.empty() ? "Untitled" : name);

    newScene->clearResourcePath();
    newScene->markClean();

    activateScene(std::move(newScene));
    return true;
}

void Engine::activateScene(std::unique_ptr<Scene::Scene> newScene)
{
    unloadScene();

    activeScene = std::move(newScene);

    renderer.setScene(activeScene.get());

    renderer.setDirectionalLight(activeScene->getDirectionalLight());

    renderer.setPointLights(activeScene->getPointLights());

    frameRenderables.clear();
}

void Engine::unloadScene()
{
    if (!activeScene)
        return;

    activeScene->clear();
    activeScene.reset();

    renderer.setScene(nullptr);
    renderer.setPointLights({});
    renderer.setDirectionalLight(Rendering::DirectionalLight{});
    frameRenderables.clear();
}

bool Engine::saveActiveScene()
{
    if (!activeScene)
    {
        Log::Print("NO ACTIVE SCENE TO SAVE", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (!activeScene->hasResourcePath())
    {
        Log::Print("ACTIVE SCENE HAS NO RESOURCEPATH, SAVE AS IS REQUIRED", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    return saveActiveSceneAs(*activeScene->getResourcePath());
}

bool Engine::saveActiveSceneAs(std::string_view resourcePath)
{
    if (!project || !activeScene)
    {
        return false;
    }

    if (!resourcePath.starts_with("res://"))
    {
        Log::Print("SCENE PATH MUST USE res://","ENGINE",LogType::LOG_ERROR);
        return false;
    }

    if (!resourcePath.ends_with(".scene"))
    {
        Log::Print("SCENE FILE MUST USE .scene EXTENSION","ENGINE",LogType::LOG_ERROR);
        return false;
    }

    auto resolvedPath =project->resolveResourcePath(resourcePath);

    if (!resolvedPath)
    {
        return false;
    }

    std::error_code error;

    std::filesystem::create_directories(resolvedPath->parent_path(), error);
    if (error)
    {
        Log::Print("FAILED TO CREATE SCENE DIRECTORY: " + resolvedPath->parent_path().string(), "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (!Scene::SceneWriter::writeScene(*activeScene, resolvedPath->string()))
    {
        return false;
    }

    activeScene->setResourcePath(std::string(resourcePath));
    activeScene->markClean();
    return true;
}

void Engine::processFixedUpdate(float fixedDt)
{
    if (!activeScene)
    {
        return;
    }

    activeScene->getWorld().each(
        [fixedDt, this]([[maybe_unused]] flecs::entity e, Scene::CharacterControllerComponent &cc) {
            if (!cc.character)
                return;

            JPH::RVec3 joltPos = cc.character->GetPosition();
            cc.prevPos = glm::vec3(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());

            bool isOnGround = cc.character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
            float gravity = -9.81f;
            float jumpVel = 5.0f;

            if (isOnGround)
            {
                cc.verticalVelocity = 0.0f;
                if (cc.pendingJump)
                    cc.verticalVelocity = jumpVel;
            }
            else
            {
                cc.verticalVelocity += gravity * fixedDt;
            }

            JPH::Vec3 velocity(cc.pendingMove.x, cc.verticalVelocity, cc.pendingMove.z);
            cc.character->SetLinearVelocity(velocity);

            JPH::CharacterVirtual::ExtendedUpdateSettings s;
            cc.character->ExtendedUpdate(fixedDt, JPH::Vec3(0, gravity, 0), s,
                                         physicsWorld.getPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(1), // MOVING
                                         physicsWorld.getPhysicsSystem()->GetDefaultLayerFilter(1), {}, {},
                                         *physicsWorld.getTempAllocator());

            joltPos = cc.character->GetPosition();
            cc.currentPos = glm::vec3(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());

            cc.pendingJump = false;
        });
}

void Engine::applySimStateToSystems()
{
    if (!activeScene)
        return;
    bool gameActive = (simState == SimulationState::Running || simState == SimulationState::Stepping);
    auto &world = activeScene->getWorld();

    // game systems disabled in editor
    auto toggle = [&](const char *name) {
        flecs::entity system = world.lookup(name);
        if (system)
        {
            if (gameActive)
                system.enable();
            else
                system.disable();
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
    if (state != EngineState::Initialized)
    {
        Log::Print(" ENGINE MUST BE INITIALISED BEFORE RUNNING", "ENGINE", LogType::LOG_ERROR);
        return;
    }

    if (!glfwWindow || !activeScene)
    {
        Log::Print(" ENGINE RUNTIME STATE IS INVALID", "ENGINE", LogType::LOG_ERROR);
        return;
    }

    state = EngineState::Running;

    lastFrame = (float)glfwGetTime();
    while (!glfwWindowShouldClose(glfwWindow))
    {
        double currentFrame = (float)glfwGetTime();
        deltaTime = (float)(currentFrame - lastFrame);
        lastFrame = currentFrame;

        Core::Input::update();
        Core::Audio::update();

        if (simState == SimulationState::Running || simState == SimulationState::Stepping)
        {
            physicsWorld.step(deltaTime);
        }

        // ecs systems are always progressing
        // the game systems are automatically handled by applySimStateToSystems()
        if (activeScene)
        {
            activeScene->getWorld().progress(deltaTime);
        }

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
        if (fbw > 0 && fbh > 0)
        {
            if (activeScene)
            {
                activeScene->getWorld().each(
                    [&](flecs::entity e, const Scene::TransformComponent &transform, const Scene::MeshComponent &mesh) {
                        if (e.has<Scene::TagActive>() && mesh.model)
                        {
                            frameRenderables.push_back({mesh.model, transform.cachedModelMatrix,
                                                        transform.cachedNormalMatrix, mesh.boundsMin, mesh.boundsMax});
                        }
                    });
            }

            renderer.render(fbw, fbh, deltaTime, frameRenderables);
        }

        glfwSwapBuffers(glfwWindow);
        glfwPollEvents();
    }

    state = EngineState::Initialized;
}

void Engine::setUpdateCallback(UpdateCallback callback, void *context)
{
    updateCallback = callback;
    updateContext = context;
}

void Engine::setRaycastCallback(RaycastCallback callback, void *context)
{
    raycastCallback = callback;
    raycastContext = context;
}
} // namespace Cthulhu