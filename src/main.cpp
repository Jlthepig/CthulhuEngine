
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

// utilities
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// window settings
glm::vec2 resolution = glm::vec2(1920.0f,1080.0f);

// frame state
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool inEditorMode = true; // start in editor/fly mode

int main()
{
    // Initialize system
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Window creation
    Window* window = Cthulhu::Core::Window::createWindow(resolution,"Cthulhu Engine");
    GLFWwindow* glfwWindow = window->getWindow();
    if (glfwWindow == NULL)
    {
        Log::Print("WINDOW IS NULL", "Main", LogType::LOG_ERROR);
        exit(1);
    }

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
         Log::Print("FAILED TO INITIALISE GLAD.", "Main", LogType::LOG_ERROR);
        return -1;
    }

    Physics::init();
    Physics::createGroundPlane();
    CharacterController::init(glm::vec3(0, 2, 0));
    Renderer renderer;
    Camera* camera = Camera::init();
    Input::init(glfwWindow, resolution);
    Input::setCamera(camera);
    renderer.init(glfwWindow, camera);

    Cthulhu::Scene::Scene scene;
    Cthulhu::Scene::SceneData sceneData = Cthulhu::Scene::SceneLoader::load(
        "assets/scenes/test.scene", scene);

    renderer.setDirectionalLight(sceneData.directionalLight);
    for (const auto& light : sceneData.pointLights)
    {
        renderer.addPointLight(light);
    }
    
    renderer.setScene(&scene);

    int fbW, fbH;
    glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    
    glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

   // Main Loop
    while (!glfwWindowShouldClose(glfwWindow)) {

        Input::update();    
        
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;  

        // toggle mode
        if (Input::isKeyPressed(GLFW_KEY_F1))
        {
            inEditorMode = !inEditorMode;
            Log::Print(inEditorMode ? "Switched to Editor Mode" : "Switched to Game Mode", "Main", LogType::LOG_INFO);
        }

        if (inEditorMode)
        {
            camera->processKeyboard(deltaTime);
        }
        else
        {
            // build movement vector from WASD
            glm::vec3 movement(0.0f);
            glm::vec3 front = camera->getFront();
            glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (Input::isKeyDown(GLFW_KEY_W)) movement += front;
            if (Input::isKeyDown(GLFW_KEY_S)) movement -= front;
            if (Input::isKeyDown(GLFW_KEY_A)) movement -= right;
            if (Input::isKeyDown(GLFW_KEY_D)) movement += right;

            if (glm::length(movement) > 0.0f)
                movement = glm::normalize(movement);

            bool jump = Input::isKeyPressed(GLFW_KEY_SPACE);

            CharacterController::update(movement, jump, deltaTime);
            glm::vec3 charPos = CharacterController::getPosition();
            charPos.y += 0.6f;
            camera->setPosition(charPos);
        }

        Physics::step(deltaTime);

        // sync physics bodies to entities
        auto& entities = scene.getEntities();
        for (auto& entity : entities)
        {
            if (entity.hasPhysicsBody)
            {
                auto transform = Physics::Physics::getBodyTransform(entity.physicsBodyId);
                entity.transform.setPosition(transform.position);
                entity.transform.setRotation(transform.rotation);

                static bool logged = false;
                if (!logged)
                {
                    Log::Print("Crate physics pos: " + std::to_string(transform.position.x) + ", " 
                        + std::to_string(transform.position.y) + ", " 
                        + std::to_string(transform.position.z), "Debug", LogType::LOG_INFO);
                    logged = true;
                }
            }
        }

        renderer.render(deltaTime);

        glfwPollEvents();
    }
    CharacterController::destroy();
    Physics::shutdown();
    renderer.shutdown();
    scene.clear();
    glfwTerminate();
    return 0;
}






