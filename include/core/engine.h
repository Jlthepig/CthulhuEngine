#pragma once
#include <string>
#include <memory>
#include <vector>
#include "glm.hpp"

#include "renderer.h"
#include "physics.h"
#include "sceneLoader.h"

struct GLFWwindow;
namespace Cthulhu::Scene { class Scene; class Camera; }
namespace Cthulhu::Core { class Window; }
namespace Cthulhu
{
    class Engine
    {
    public:
        using UpdateCallback = void(*)(void* context, float deltaTime);
        using RaycastCallback = void(*)(void* context, const Physics::RaycastHitInfo& hit);
        
        void init(const char* title, glm::vec2 resolution);
        void loadScene(const std::string& path);
        void setUpdateCallback(UpdateCallback callback, void* context = nullptr);
        void setRaycastCallback(RaycastCallback callback, void* context = nullptr);
        void processFixedUpdate(float fixedDt);
        void run();
        void shutdown();
        Cthulhu::Scene::Camera* getCamera();
        Cthulhu::Core::Window* getWindow() { return window; }
        Cthulhu::Rendering::Renderer& getRenderer() {return renderer;}
        Cthulhu::Physics::PhysicsWorld& getPhysicsWorld() { return physicsWorld; }
        Cthulhu::Scene::Scene& getScene() { return *scene; }

    private:
        Rendering::Renderer renderer;
        Cthulhu::Physics::PhysicsWorld physicsWorld;
        Cthulhu::Scene::Camera* camera = nullptr;
        Cthulhu::Core::Window* window = nullptr;
        GLFWwindow* glfwWindow = nullptr;
        std::unique_ptr<Cthulhu::Scene::Scene> scene; 
        Cthulhu::Scene::SceneData sceneData;
        std::vector<Rendering::Renderable> frameRenderables;
        
        UpdateCallback updateCallback = nullptr;
        void* updateContext = nullptr;
        RaycastCallback raycastCallback = nullptr;
        void* raycastContext = nullptr;

        float deltaTime = 0.0f;
        double lastFrame = 0.0f;
        double fpsTimer = 0.0f;
        int frameCount = 0;
        float displayFPS = 0.0f;
    };
}