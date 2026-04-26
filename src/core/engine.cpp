
// standard libraries
#include "sceneLoader.h"
#include <cstdlib>

// third party libraries
#define STB_IMAGE_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "stb_image.h"
#include "log_utils.hpp"
#include "Jolt/Jolt.h"


// engine
#include "engine.h"
#include "camera.h"
#include "window.h"
#include "input.h"
#include "renderer.h"
#include "scene.h"
#include "physics.h"
#include "characterController.h"

// engine types
using Cthulhu::Scene::Camera;
using Cthulhu::Core::Window;
using Cthulhu::Core::Input;
using Cthulhu::Rendering::Renderer;
using Cthulhu::Physics::Physics;
using Cthulhu::Physics::CharacterController;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;


// Global state
glm::vec2 resolution;
float deltaTime = 0.0f;
float lastFrame = 0.0f;


// Subsystems
static Renderer renderer;
static Camera* camera = nullptr;
static Window* window = nullptr;
static GLFWwindow* glfwWindow = nullptr;
static Cthulhu::Scene::Scene scene;
static Cthulhu::Scene::SceneData sceneData;
static Cthulhu::Engine::UpdateCallback gameUpdateCallback = nullptr;

namespace EngineConfig
{
    // OpenGL version
    constexpr int OPENGL_VERSION_MAJOR = 3;
    constexpr int OPENGL_VERSION_MINOR = 3;
}

namespace Cthulhu
{
    void Engine::init(const char* title, glm::vec2 resolution)
    {
        // Initialize system
        if (!glfwInit()) 
        {
            Log::Print("CANNOT INITIALIZE GLFW", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, EngineConfig::OPENGL_VERSION_MAJOR);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, EngineConfig::OPENGL_VERSION_MINOR);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Window creation
        window = Cthulhu::Core::Window::createWindow(resolution, title);
        glfwWindow = window->getWindow();
        if (glfwWindow == NULL)
        {
            Log::Print("WINDOW IS NULL", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }

            // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            Log::Print("FAILED TO INITIALISE GLAD.", "ENGINE", LogType::LOG_ERROR);
            exit(1);
        }

        Physics::Physics::init();
        Physics::Physics::createGroundPlane();
        camera = Camera::init();
        Input::init(glfwWindow, resolution);
        Input::setCamera(camera);
        renderer.init(glfwWindow, camera);

        int fbW, fbH;
        glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void Engine::loadScene(const std::string &path)
    {
        sceneData = Cthulhu::Scene::SceneLoader::load(path, scene);

        renderer.setDirectionalLight(sceneData.directionalLight);
        for (const auto& light : sceneData.pointLights)
        {
            renderer.addPointLight(light);
        }
    
        renderer.setScene(&scene);
    }

    void Engine::setUpdateCallback(UpdateCallback callback)
    {
         gameUpdateCallback = callback;
    }

    Scene::Camera* Engine::getCamera() { return camera; }

    void Engine::run()
    {
       while (!glfwWindowShouldClose(glfwWindow))
       {
            Input::update();    
        
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            Physics::Physics::step(deltaTime);

            // sync physics bodies to entities
            auto& entities = scene.getEntities();
            for (auto& entity : entities)
            {
                if (entity.hasPhysicsBody)
                {
                    auto transform = Physics::Physics::getBodyTransform(entity.physicsBodyId);
                    entity.transform.setPosition(transform.position);
                    entity.transform.setRotation(transform.rotation);
                }
            }

            if (gameUpdateCallback) 
            {
                gameUpdateCallback(deltaTime);
            }

            renderer.render(deltaTime);
            glfwPollEvents();
       }
    }

    void Engine::shutdown()
    {
        Physics::Physics::shutdown();
        renderer.shutdown();
        scene.clear();
        glfwTerminate();
    }
}