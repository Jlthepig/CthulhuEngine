#include "components.h"
#include "flecs.h"
#include "sceneLoader.h"
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "ext/matrix_transform.hpp"
#include "stb_image.h"
#include "log_utils.hpp"
#include "Jolt/Jolt.h"

#include "engine.h"
#include "camera.h"
#include "window.h"
#include "input.h"
#include "renderer.h"
#include "scene.h"
#include "physics.h"
#include "characterController.h"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

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

        camera = Scene::Camera::init();
        Core::Input::init(glfwWindow, resolution);

        Cthulhu::Rendering::RenderConfig renderConfig;
        renderer.init(glfwWindow, camera, renderConfig);

        int fbW, fbH;
        glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // register physics sync system runs before transform system
        scene.getWorld().system<Scene::TransformComponent, const Scene::PhysicsComponent>("PhysicsSyncSystem")
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
        scene.getWorld().system<Cthulhu::Scene::TransformComponent>("TransformSystem")
            .each([](Cthulhu::Scene::TransformComponent& transform)
            {
                    if (transform.matrixDirty)
                    {
                        using namespace glm;
                        transform.cachedModelMatrix = mat4(1.0f);
                        transform.cachedModelMatrix = translate(transform.cachedModelMatrix, transform.position);
                        transform.cachedModelMatrix = rotate(transform.cachedModelMatrix, transform.rotation.x, vec3(1.0f, 0.0f, 0.0f));
                        transform.cachedModelMatrix = rotate(transform.cachedModelMatrix, transform.rotation.y, vec3(0.0f, 1.0f, 0.0f));
                        transform.cachedModelMatrix = rotate(transform.cachedModelMatrix, transform.rotation.z, vec3(0.0f, 0.0f, 1.0f));
                        transform.cachedModelMatrix = scale(transform.cachedModelMatrix, transform.scale);
                        transform.matrixDirty = false;
                    }
                }
            );

    }

    void Engine::loadScene(const std::string &path)
    {
        sceneData = Scene::SceneLoader::load(path, scene, physicsWorld);

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
            Core::Input::update();    
        
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            physicsWorld.step(deltaTime);
            scene.getWorld().progress(deltaTime);
            std::vector<Rendering::Renderable> renderables;
            scene.getWorld().each([&](flecs::entity e, const Scene::TransformComponent& transform, const Scene::MeshComponent& mesh) {
                if (e.has<Scene::TagActive>() && mesh.model) {
                    renderables.push_back({
                        mesh.model,
                        transform.cachedModelMatrix,
                        transform.cachedNormalMatrix,
                        mesh.boundsMin,
                        mesh.boundsMax
                    });
                }
            });

            if (gameUpdateCallback) 
            {
                gameUpdateCallback(deltaTime);
            }

            renderer.render(deltaTime, renderables);
            glfwPollEvents();
       }
    }

    void Engine::shutdown()
    {
        physicsWorld.shutdown();
        renderer.shutdown();
        scene.clear();
        glfwTerminate();
    }
}