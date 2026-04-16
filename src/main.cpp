
// standard libraries
#include "entity.h"
#include "model.h"
#include <cstdlib>

// third party libraries
#define STB_IMAGE_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "stb_image.h"
#include "log_utils.hpp"

// engine
#include "camera.h"
#include "window.h"
#include "input.h"
#include "renderer.h"
#include "scene.h"
#include "modelLoader.h"

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

    Cthulhu::Rendering::Model fishModel;
    fishModel = Cthulhu::Rendering::ModelLoader::loadGltf("assets/models/BarramundiFish.glb");

    Cthulhu::Scene::Scene scene;
    Cthulhu::Scene::Entity fishEntity;
    fishEntity.name = "Fish";
    fishEntity.model = &fishModel;  // point to the model
    fishEntity.transform.setPosition(glm::vec3(0.0f));
    fishEntity.transform.setScale(glm::vec3(1.0f));
    fishEntity.bounds.min = glm::vec3(-2.0f, -2.0f, -2.0f);
    fishEntity.bounds.max = glm::vec3(2.0f, 2.0f, 2.0f);

    Cthulhu::Scene::Entity& fish = scene.addEntity(fishEntity);
    
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

        fish.transform.setRotation(glm::vec3(0.0f, 0.59f * currentFrame, 0.0f));
        glfwPollEvents();
    }
    renderer.shutdown();
    fishModel.destroy();
    glfwTerminate();
    return 0;
}






