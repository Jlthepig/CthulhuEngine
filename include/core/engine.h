#pragma once
#include <functional>
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
        using UpdateCallback = std::function<void(float deltaTime)>;
        
        void init(const char* title, glm::vec2 resolution);
        void loadScene(const std::string& path);
        void setUpdateCallback(UpdateCallback callback);
        void run();
        void shutdown();
        Cthulhu::Scene::Camera* getCamera();
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
        
        UpdateCallback gameUpdateCallback = nullptr;

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;
    };
}