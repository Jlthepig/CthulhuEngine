#pragma once

#include "camera.h"
#include "renderer.h"
#include "physics.h"
#include "scene.h"
#include "sceneLoader.h"
#include "window.h"
#include <functional>
#include <string>

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
        Cthulhu::Scene::Scene& getScene() { return scene; }

    private:
        // The Engine now OWNS this data
        Cthulhu::Rendering::Renderer renderer;
        Cthulhu::Physics::PhysicsWorld physicsWorld;
        Cthulhu::Scene::Camera* camera = nullptr;
        Cthulhu::Core::Window* window = nullptr;
        GLFWwindow* glfwWindow = nullptr;
        Cthulhu::Scene::Scene scene;
        Cthulhu::Scene::SceneData sceneData;
        std::vector<Rendering::Renderable> frameRenderables;
        
        UpdateCallback gameUpdateCallback = nullptr;

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;
    };
}