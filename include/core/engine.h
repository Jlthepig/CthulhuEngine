#pragma once

#include <string_view>
#include <memory>
#include <vector>
#include <filesystem>
#include <optional>

#include "renderer.h"
#include "physics.h"
#include "project.h"

struct GLFWwindow;
namespace Cthulhu::Scene { class Scene; class Camera; }
namespace Cthulhu::Core { class Window; }
namespace Cthulhu
{   
    enum SimulationState
    {
        Running,
        Paused,
        Stepping,
    };
    class Engine
    {
    public:
        using UpdateCallback = void(*)(void* context, float deltaTime);
        using RaycastCallback = void(*)(void* context, const Physics::RaycastHitInfo& hit);
        
        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        bool init(const std::filesystem::path& projectFilePath);
        void loadScene(std::string_view resourcePath);
        void setUpdateCallback(UpdateCallback callback, void* context = nullptr);
        void setRaycastCallback(RaycastCallback callback, void* context = nullptr);
        void processFixedUpdate(float fixedDt);
        void run();
        void shutdown();

        void setSimulationState(SimulationState state);
        SimulationState getSimulationState() const {return simState;}
        // single frame queue reverts back to paused after consumption
        void stepSimulation();

        Scene::Camera* getCamera() {return camera;}
        Core::Window* getWindow() {return window;}

        Rendering::Renderer& getRenderer() {return renderer;}
        Physics::PhysicsWorld& getPhysicsWorld() { return physicsWorld; }
        Scene::Scene& getScene() { return *scene; }
        
        const Project::Project* getProject() const {return project ? &*project : nullptr;}

        float getDeltaTime() const { return deltaTime; }

        void triggerRaycastCallback(const Physics::RaycastHitInfo& hit) {
            if (raycastCallback) {raycastCallback(raycastContext, hit);}
        }

    private:
        std::optional<Cthulhu::Project::Project> project;
        std::filesystem::path engineResourceRoot;

        Rendering::Renderer renderer;
        Physics::PhysicsWorld physicsWorld;

        Scene::Camera* camera = nullptr;
        Core::Window* window = nullptr;
        GLFWwindow* glfwWindow = nullptr;

        std::unique_ptr<Cthulhu::Scene::Scene> scene; 
        std::vector<Rendering::Renderable> frameRenderables;
        
        UpdateCallback updateCallback = nullptr;
        void* updateContext = nullptr;

        RaycastCallback raycastCallback = nullptr;
        void* raycastContext = nullptr;

        SimulationState simState = SimulationState::Running;
        void applySimStateToSystems(); // toggle systems based on the simState
        

        float deltaTime = 0.0f;
        double lastFrame = 0.0f;
    };
}