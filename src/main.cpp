
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

// engine types
using Cthulhu::Scene::Camera;
using Cthulhu::Core::Window;
using Cthulhu::Core::Input;
using Cthulhu::Rendering::Renderer;

// utilities
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// window settings
glm::vec2 resolution = glm::vec2(1920.0f,1080.0f);

// frame state
float deltaTime = 0.0f;
float lastFrame = 0.0f;

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
    
        camera->processKeyboard(deltaTime);
        renderer.render(deltaTime);

        glfwPollEvents();
    }
    renderer.shutdown();
    scene.clear();
    glfwTerminate();
    return 0;
}






