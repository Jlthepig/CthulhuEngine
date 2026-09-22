#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "physics.h"
#include "project.h"
#include "renderer.h"

struct GLFWwindow;
namespace Cthulhu::Scene
{
class Scene;
class Camera;
} // namespace Cthulhu::Scene
namespace Cthulhu::Core
{
class Window;
}
namespace Cthulhu
{
enum class EngineState
{
    Uninitialized,
    Initialized,
    Running,
    ShuttingDown
};

enum SimulationState
{
    Running,
    Paused,
    Stepping,
};
class Engine
{
  public:
    using UpdateCallback = void (*)(void *context, float deltaTime);
    using RaycastCallback = void (*)(void *context, const Physics::RaycastHitInfo &hit);

    Engine() = default;
    ~Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    bool init(const std::filesystem::path &projectFilePath);
    void run();
    void shutdown();

    bool loadScene(std::string_view resourcePath);
    bool createEmptyScene(const std::string &name = "Untitled");
    void unloadScene();

    void setUpdateCallback(UpdateCallback callback, void *context = nullptr);
    void setRaycastCallback(RaycastCallback callback, void *context = nullptr);
    void triggerRaycastCallback(const Physics::RaycastHitInfo &hit)
    {
        if (raycastCallback)
        {
            raycastCallback(raycastContext, hit);
        }
    }

    void processFixedUpdate(float fixedDt);

    EngineState getState() const
    {
        return state;
    }
    bool isInitialized() const
    {
        return state != EngineState::Uninitialized;
    }

    void setSimulationState(SimulationState state);
    SimulationState getSimulationState() const
    {
        return simState;
    }
    void stepSimulation();

    Scene::Camera *getCamera()
    {
        return camera;
    }
    Core::Window *getWindow()
    {
        return window;
    }

    Rendering::Renderer &getRenderer()
    {
        return renderer;
    }
    Physics::PhysicsWorld &getPhysicsWorld()
    {
        return physicsWorld;
    }

    Scene::Scene *getActiveScene()
    {
        return activeScene.get();
    }

    const Scene::Scene *getActiveScene() const
    {
        return activeScene.get();
    }

    bool hasActiveScene() const
    {
        return activeScene != nullptr;
    }

    const Project::Project *getProject() const
    {
        return project ? &*project : nullptr;
    }

    float getDeltaTime() const
    {
        return deltaTime;
    }

  private:
    std::optional<Cthulhu::Project::Project> project;
    std::filesystem::path engineResourceRoot;

    Rendering::Renderer renderer;
    Physics::PhysicsWorld physicsWorld;

    Scene::Camera *camera = nullptr;
    Core::Window *window = nullptr;
    GLFWwindow *glfwWindow = nullptr;

    std::unique_ptr<Scene::Scene> createSceneInstance();
    void activateScene(std::unique_ptr<Scene::Scene> newScene);

    std::unique_ptr<Cthulhu::Scene::Scene> activeScene;
    std::vector<Rendering::Renderable> frameRenderables;

    UpdateCallback updateCallback = nullptr;
    void *updateContext = nullptr;

    RaycastCallback raycastCallback = nullptr;
    void *raycastContext = nullptr;

    EngineState state = EngineState::Uninitialized;
    bool glfwInitialized = false;
    bool physicsInitialized = false;
    bool audioInitialized = false;
    bool rendererInitialized = false;

    SimulationState simState = SimulationState::Running;
    void applySimStateToSystems(); // toggle systems based on the simState

    float deltaTime = 0.0f;
    double lastFrame = 0.0f;
};
} // namespace Cthulhu