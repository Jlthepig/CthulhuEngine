#include <algorithm>
#include <cstdlib>

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <glad.h>
#include <stb_image.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "applicationPaths.hpp"
#include "assetType.hpp"
#include "audio.hpp"
#include "builtinComponents.hpp"
#include "camera.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "input.hpp"
#include "jsonWriter.hpp"
#include "log_utils.hpp"
#include "physics.hpp"
#include "pathEncoding.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "sceneLoader.hpp"
#include "scopedFlag.hpp"
#include "systemRegistry.hpp"
#include "transformUtils.hpp"
#include "window.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

static void physicsFixedUpdateCallback(void *context, float fixedDt)
{
    static_cast<Cthulhu::Engine *>(context)->processFixedUpdate(fixedDt);
}

namespace Cthulhu
{

using Core::ScopedFlag;

bool Engine::transitionBlocked() const
{
    return progressingSimulation || dispatchingRegistrations || dispatchingEdits ||
           state == EngineState::ShuttingDown ||
           (activeScene && ecs_stage_is_readonly(activeScene->getWorld().c_ptr())) ||
           (activeScene && ecs_is_deferred(activeScene->getWorld().c_ptr()));
}

bool Engine::requestTransition(EngineTransition transition, std::string argument)
{
    if (!isInitialized() || state == EngineState::ShuttingDown)
    {
        return false;
    }

    if (pendingTransition)
    {
        Log::Print("A LIFECYCLE TRANSITION IS ALREADY QUEUED", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (transition == EngineTransition::LoadScene && argument.empty())
    {
        return false;
    }

    pendingTransition = TransitionRequest{transition, std::move(argument)};
    return true;
}

void Engine::applyPendingTransition()
{
    if (!pendingTransition || transitionBlocked() || state == EngineState::Uninitialized)
    {
        return;
    }

    TransitionRequest request = std::move(*pendingTransition);
    pendingTransition.reset();

    bool succeeded = true;
    switch (request.transition)
    {
        case EngineTransition::LoadScene:
            succeeded = loadScene(request.argument);
            break;
        case EngineTransition::CreateEmptyScene:
            succeeded = createEmptyScene(request.argument);
            break;
        case EngineTransition::UnloadScene:
            unloadScene();
            break;
        case EngineTransition::Play:
            succeeded = play();
            break;
        case EngineTransition::Stop:
            succeeded = stop();
            break;
        case EngineTransition::Shutdown:
            shutdown();
            break;
        default:
            succeeded = false;
            break;
    }

    if (!succeeded)
    {
        Log::Print("QUEUED LIFECYCLE TRANSITION FAILED", "ENGINE", LogType::LOG_ERROR);
    }
} 

Engine::Engine()
{
    if (!Scene::registerBuiltinComponents(componentRegistry))
    {
        Log::Print("FAILED TO REGISTER BUILT-IN COMPONENTS", "ENGINE", LogType::LOG_ERROR);
    }
}

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
    if (!assetManager.setProject(&*project))
    {
        shutdown();
        return false;
    }
    assetManager.refreshRegistry();

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
        Log::Print("FAILED TO INITIALISE GLAD", "ENGINE", LogType::LOG_ERROR);
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

    if (!physicsWorld.init(physicsConfig))
    {
        Log::Print("FAILED TO INITIALISE PHYSICS", "ENGINE", LogType::LOG_ERROR);
        shutdown();
        return false;
    }
    physicsInitialized = true;

    camera = Scene::Camera::init();

    Core::Input::init(glfwWindow, resolution);
    if (!Core::Audio::init())
    {
        shutdown();
        return false;
    }
    audioInitialized = true;

    if (!renderer.init(glfwWindow, camera, renderConfig))
    {
        shutdown();
        return false;
    }
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

    if (transitionBlocked())
    {
        Log::Print("SHUTDOWN REQUIRES A FRAME BOUNDARY", "ENGINE", LogType::LOG_ERROR);
        return;
    }

    pendingTransition.reset();

    if (state == EngineState::Initialized && !glfwInitialized)
    {
        return;
    }

    state = EngineState::ShuttingDown;
    editSnapshot.reset();

    updateCallback = nullptr;
    updateContext = nullptr;

    raycastCallback = nullptr;
    raycastContext = nullptr;

    if (activeScene)
    {
        releaseActiveScene();
        ++sceneGeneration;
    }

    assetManager.shutdown();

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

    Core::Input::shutdown();

    camera = nullptr;
    window = nullptr;
    glfwWindow = nullptr;

    project.reset();

    if (glfwInitialized)
    {
        Core::Window::destroyAll();
        glfwTerminate();
        glfwInitialized = false;
    }

    frameRenderables.clear();

    deltaTime = 0.0f;
    lastFrame = 0.0;

    state = EngineState::Uninitialized;
}

std::unique_ptr<Scene::Scene> Engine::createSceneInstance()
{
    auto newScene = std::make_unique<Scene::Scene>(componentRegistry);

    Scene::RegisterCoreSystems(newScene->getWorld(), this);

    {
        const ScopedFlag dispatch(dispatchingRegistrations);

        for (const auto &entry : systemRegistrations)
        {
            entry.registration(entry.contex, newScene->getWorld(), *this);
        }
    }

    return newScene;
}

bool Engine::loadScene(std::string_view resourcePath)
{
    if (transitionBlocked())
    {
        Log::Print("LIFECYCLE TRANSITION REQUIRES A FRAME BOUNDARY","ENGINE",LogType::LOG_ERROR);
        return false;
    }

    if (!project)
    {
        Log::Print("CANNOT LOAD SCENE WITHOUT AN ACTIVE PROJECT", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (Assets::getAssetType(resourcePath) != Assets::AssetType::Scene)
    {
        Log::Print("NOT A SCENE FILE: " + std::string(resourcePath), "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto resolvedPath = project->resolveResourcePath(resourcePath);

    if (!resolvedPath)
    {
        return false;
    }

    auto newScene = createSceneInstance();
    if (!Scene::SceneLoader::load(Utils::pathToUtf8(*resolvedPath), *newScene, assetManager.getRegistry()))
    {
        Log::Print("FAILED TO LOAD SCENE: " + std::string(resourcePath), "ENGINE", LogType::LOG_ERROR);

        newScene.reset();
        assetManager.collectUnusedAssets();
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
    if (transitionBlocked())
    {
        Log::Print("LIFECYCLE TRANSITION REQUIRES A FRAME BOUNDARY","ENGINE",LogType::LOG_ERROR);
        return false;
    }

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

bool Engine::restoreEditScene(const Scene::SceneSnapshot &snapshot)
{
    auto restored = createSceneInstance();
    if (!restored->restoreScene(snapshot))
    {
        restored.reset();
        assetManager.collectUnusedAssets();
        return false;
    }

    activateScene(std::move(restored));
    return true;
}

void Engine::activateScene(std::unique_ptr<Scene::Scene> newScene, bool newDocument)
{
    releaseActiveScene();

    activeScene = std::move(newScene);
    if (newDocument)
    {
        ++sceneGeneration;
    }

    renderer.setScene(activeScene.get());

    renderer.setDirectionalLight(activeScene->getDirectionalLight());

    renderer.setPointLights(activeScene->getPointLights());

    frameRenderables.clear();

    applySimStateToSystems();
}

void Engine::unloadScene()
{
    if (transitionBlocked())
    {
        Log::Print("LIFECYCLE TRANSITION REQUIRES A FRAME BOUNDARY","ENGINE",LogType::LOG_ERROR);
        return;
    }

    if (!activeScene)
        return;

    releaseActiveScene();
    ++sceneGeneration;
}

void Engine::releaseActiveScene()
{
    if (!activeScene)
        return;

    activeScene->clear();
    activeScene.reset();

    assetManager.collectUnusedAssets();

    renderer.setScene(nullptr);
    renderer.setPointLights({});
    renderer.setDirectionalLight(Rendering::DirectionalLight{});
    frameRenderables.clear();
}

std::optional<std::string_view> Engine::unsavedEditSceneName() const noexcept
{
    if (editSnapshot)
    {
        if (editSnapshot->dirty)
            return editSnapshot->name;

        return std::nullopt;
    }

    if (activeScene && activeScene->needsSave())
        return activeScene->getName();

    return std::nullopt;
}

bool Engine::saveActiveScene()
{
    if (!activeScene)
    {
        Log::Print("NO ACTIVE SCENE TO SAVE", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (isPlaying())
    {
        Log::Print("CANNOT SAVE WHILE PLAYING", "ENGINE", LogType::LOG_ERROR);
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
    if (isPlaying())
    {
        Log::Print("CANNOT SAVE WHILE PLAYING", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (!project || !activeScene)
    {
        return false;
    }

    if (!resourcePath.starts_with("res://"))
    {
        Log::Print("SCENE PATH MUST USE res://", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (Assets::getAssetType(resourcePath) != Assets::AssetType::Scene)
    {
        Log::Print("SCENE FILE MUST USE .scene EXTENSION", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto resolvedPath = project->resolveResourcePath(resourcePath);

    if (!resolvedPath)
    {
        return false;
    }

    std::error_code error;

    std::filesystem::create_directories(resolvedPath->parent_path(), error);
    if (error)
    {
        Log::Print("FAILED TO CREATE SCENE DIRECTORY: " + resolvedPath->parent_path().string(),"ENGINE",LogType::LOG_ERROR);
        return false;
    }

    if (!Scene::SceneWriter::writeScene(*activeScene, Utils::pathToUtf8(*resolvedPath), assetManager.getRegistry()))
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

    activeScene->getWorld().each([this](flecs::entity entity, const Scene::PhysicsComponent &physics,
                const Scene::PhysicsRuntimeComponent &runtime,
                const Scene::TransformComponent &) {
            if (physics.type != Scene::PhysicsBodyType::Static || !runtime.bodyId || entity.has<Scene::CharacterControllerComponent>())
            {
                return;
            }

            const Scene::WorldTransform world = Scene::getWorldTransform(entity);

            physicsWorld.setStaticBodyTransform(
                *runtime.bodyId,
                Physics::BodyTransform{
                    glm::vec3(world.matrix[3]),
                    world.rotation,
                });
        });

    activeScene->getWorld().each([fixedDt, this](const Scene::CharacterControllerComponent &config,
                                                 Scene::CharacterControllerRuntimeComponent &runtime) {
        if (!runtime.character)
        {
            return;
        }

        JPH::RVec3 charPos = runtime.character->GetPosition();
        runtime.prevPos = glm::vec3(charPos.GetX(), charPos.GetY(), charPos.GetZ());

        const bool isOnGround = runtime.character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;

        if (isOnGround)
        {
            runtime.verticalVelocity = 0.0f;
            if (runtime.pendingJump)
            {
                runtime.verticalVelocity = config.jumpVelocity;
            }
        }
        else
        {
            runtime.verticalVelocity += config.gravity * fixedDt;
        }

        const JPH::Vec3 velocity(runtime.pendingMove.x, runtime.verticalVelocity, runtime.pendingMove.z);

        runtime.character->SetLinearVelocity(velocity);

        JPH::CharacterVirtual::ExtendedUpdateSettings settings;

        runtime.character->ExtendedUpdate(fixedDt,
                                          JPH::Vec3(0.0f, config.gravity, 0.0f),
                                          settings,
                                          physicsWorld.getPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(1),
                                          physicsWorld.getPhysicsSystem()->GetDefaultLayerFilter(1),
                                          {},
                                          {},
                                          *physicsWorld.getTempAllocator());

        charPos = runtime.character->GetPosition();

        runtime.currentPos = glm::vec3(charPos.GetX(), charPos.GetY(), charPos.GetZ());
        runtime.pendingJump = false;
    });
}

void Engine::applySimStateToSystems()
{
    if (!activeScene)
        return;
    const bool gameActive = isSimulating();
    auto &world = activeScene->getWorld();

    std::vector<flecs::entity> systems;
    world.query_builder()
        .with<Scene::GameplaySystem>()
        .with(flecs::Disabled)
        .optional()
        .build()
        .each([&](flecs::entity system) { systems.push_back(system); });

    for (flecs::entity system : systems)
    {
        if (gameActive)
        {
            system.enable();
        }
        else
        {
            system.disable();
        }
    }
}

void Engine::setSimulationState(SimulationState state)
{
    simState = state;
    applySimStateToSystems();
}

void Engine::stepSimulation()
{
    if (worldMode != WorldMode::Play)
    {
        return;
    }

    simState = SimulationState::Stepping;
    applySimStateToSystems();
}

void Engine::setWorldMode(WorldMode mode)
{
    if (worldMode == mode)
    {
        return;
    }

    worldMode = mode;
    applySimStateToSystems();
}

void Engine::addSystemRegistration(SystemRegistration registration, void *context)
{
    if (!registration)
    {
        return;
    }

    if (transitionBlocked())
    {
        Log::Print("CANNOT REGISTER SYSTEMS DURING CALLBACK DISPATCH OR SIMULATION","ENGINE",LogType::LOG_ERROR);
        return;
    }

    systemRegistrations.push_back({registration, context});
    if (activeScene)
    {
        const ScopedFlag dispatch(dispatchingRegistrations);
        registration(context, activeScene->getWorld(), *this);
        applySimStateToSystems();
    }
}

bool Engine::play()
{
    if (transitionBlocked())
    {
        Log::Print("LIFECYCLE TRANSITION REQUIRES A FRAME BOUNDARY","ENGINE",LogType::LOG_ERROR);
        return false;
    }

    if (transformOwner)
    {
        Log::Print("CANNOT PLAY DURING TRANSFORM PREVIEW", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    if (!activeScene || isPlaying() || worldMode != WorldMode::Edit)
    {
        Log::Print("PLAY NEEDS AN ACTIVE SCENE IN EDIT MODE", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto snapshot = std::make_unique<Scene::SceneSnapshot>(activeScene->captureScene());

    auto playScene = createSceneInstance();
    if (!playScene->restoreScene(*snapshot))
    {
        Log::Print("FAILED TO BUILD THE PLAY SCENE", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    editGeneration = sceneGeneration;
    editSnapshot = std::move(snapshot);
    worldMode = WorldMode::Play;
    simState = SimulationState::Running;
    activateScene(std::move(playScene), false);

    Log::Print("Play", "ENGINE", LogType::LOG_INFO);
    return true;
}

bool Engine::stop()
{
    if (transitionBlocked())
    {
        Log::Print("LIFECYCLE TRANSITION REQUIRES A FRAME BOUNDARY","ENGINE",LogType::LOG_ERROR);
        return false;
    }

    if (!isPlaying())
    {
        Log::Print("STOP CALLED WHILE NOT PLAYING", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    auto editScene = createSceneInstance();
    if (!editScene->restoreScene(*editSnapshot))
    {
        Log::Print("FAILED TO RESTORE THE EDIT SCENE, STILL PLAYING", "ENGINE", LogType::LOG_ERROR);
        return false;
    }

    editSnapshot.reset();
    worldMode = WorldMode::Edit;
    activateScene(std::move(editScene), false);
    sceneGeneration = editGeneration;

    Log::Print("Stop", "ENGINE", LogType::LOG_INFO);
    return true;
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
    lastFrame = glfwGetTime();

    while (!shouldClose())
    {
        update();
        render();
        present();
    }

    if (state == EngineState::Running)
    {
        state = EngineState::Initialized;
    }
}

bool Engine::shouldClose() const
{
    return !glfwWindow || glfwWindowShouldClose(glfwWindow);
}

void Engine::update()
{
    if (state == EngineState::Uninitialized || !glfwWindow)
    {
        return;
    }

    applyPendingTransition();
    if (state == EngineState::Uninitialized || !glfwWindow)
    {
        return;
    }

    const double currentFrame = glfwGetTime();
    if (lastFrame <= 0.0)
    {
        lastFrame = currentFrame; // first frame
    }
    deltaTime = static_cast<float>(currentFrame - lastFrame);
    lastFrame = currentFrame;

    Core::Input::update();
    Core::Audio::update();

    const float simulationDt = simState == SimulationState::Stepping ? physicsWorld.fixedDeltaTime() : deltaTime;

    {
        const ScopedFlag progress(progressingSimulation);

        if (isSimulating())
        {
            physicsWorld.step(simulationDt);
        }

        if (activeScene)
        {
            activeScene->getWorld().progress(simulationDt);
        }
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

    applyPendingTransition();
}

void Engine::resetFrameTiming() noexcept
{
    deltaTime = 0.0f;
    lastFrame = glfwInitialized ? glfwGetTime() : 0.0;
}

void Engine::refreshSceneTransforms()
{
    if (!activeScene || progressingSimulation)
        return;

    auto &world = activeScene->getWorld();
    const auto system = world.lookup("TransformSystem");

    if (system)
    {
        ecs_run(world.c_ptr(), system.id(), 0.0f, nullptr);
    }
}

void Engine::render(std::span<const Rendering::OverlayVertex> overlayVertices, const Rendering::ModelPreview &preview)
{
    if (state == EngineState::Uninitialized || !glfwWindow)
    {
        return;
    }

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(glfwWindow, &fbw, &fbh);

    frameRenderables.clear();
    if (fbw <= 0 || fbh <= 0)
    {
        return;
    }

    if (activeScene)
    {
        activeScene->getWorld().each([&](flecs::entity e,
                                         const Scene::TransformComponent &transform,
                                         const Scene::MeshComponent &,
                                         const Scene::MeshRuntimeComponent &meshRuntime) {
            if (!e.has<Scene::TagActive>())
            {
                return;
            }

            Rendering::Model *model = assetManager.getModel(meshRuntime.model);
            if (!model)
            {
                return;
            }

            const auto *identity = e.try_get<Scene::EntityIdentityComponent>();

            frameRenderables.push_back({model,
                                        transform.cachedModelMatrix,
                                        transform.cachedNormalMatrix,
                                        model->boundsMin,
                                        model->boundsMax,
                                        identity ? identity->id : Scene::EntityId{}});
        });
    }

    int x = 0;
    int y = 0;
    int w = fbw;
    int h = fbh;
    if (viewportWidth > 0 && viewportHeight > 0)
    {
        x = std::clamp(viewportX, 0, fbw - 1);
        y = std::clamp(viewportY, 0, fbh - 1);
        w = std::min(viewportWidth, fbw - x);
        h = std::min(viewportHeight, fbh - y);
    }

    renderer.render(static_cast<unsigned int>(w),
                    static_cast<unsigned int>(h),
                    deltaTime,
                    frameRenderables,
                    x,
                    fbh - (y + h),
                    overlayVertices,
                    preview);
}

void Engine::setViewportRect(int x, int y, int width, int height)
{
    viewportX = x;
    viewportY = y;
    viewportWidth = width;
    viewportHeight = height;
}

void Engine::present()
{
    if (!glfwWindow)
    {
        return;
    }

    glfwSwapBuffers(glfwWindow);
    glfwPollEvents();
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
