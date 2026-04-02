
// standard libraries
#include "ext/matrix_transform.hpp"
#include "ext/vector_float3.hpp"
#include "trigonometric.hpp"
#include <cstdlib>
#include <vector>

// third party libraries
#define STB_IMAGE_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "stb_image.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "log_utils.hpp"

// engine
#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "texture.h"
#include "modelLoader.h"
#include "window.h"
#include "grid.h"
#include "cthulhuInput.h"
#include "transform.h"
// engine types
using Cthulhu::Rendering::Shader;
using Cthulhu::Scene::Camera;
using Cthulhu::Rendering::Mesh;
using Cthulhu::Rendering::Texture;
using Cthulhu::Rendering::Model;
using Cthulhu::Rendering::ModelLoader;
using Cthulhu::Rendering::GridLines;
using Cthulhu::Core::Window;
using Cthulhu::Core::Input;
using Cthulhu::Scene::Transform;

// utilities
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// window settings
glm::vec2 resolution = glm::vec2(1920.0f,1080.0f);

// frame state
double deltaTime = 0.0f;
double lastFrame = 0.0f;

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

    

    Camera* camera = Camera::init();
    Input::setCamera(camera);
    Log::Print("start log", "Main", LogType::LOG_INFO);
    
    Texture texture;
    

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
         Log::Print("FAILED TO INITIALISE GLAD.", "Main", LogType::LOG_ERROR);
        return -1;
    }

    Input::init(glfwWindow, resolution);
    // loading resources
    
    GridLines grid;
    grid.setupGrid(256);

    Shader basicShader;
    Shader gridLines_Shader;
    Model boxModel = ModelLoader::loadGltf("assets/models/BarramundiFish.glb");
    texture.load("./assets/images/lava.png");
    
    basicShader.load("shaders/basic.vertex","shaders/basic.fragment");
    gridLines_Shader.load("shaders/basic.vertex", "shaders/grid.fragment");

    int fbW, fbH;
    
    glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // scene setup
    
    glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glm::mat4 projection = glm::perspective(camera->getFov(), (float)window->getWidth()/(float)window->getHeight(), 0.1f, 100.0f);
    glm::mat4 view{};
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Main Loop
    while (!glfwWindowShouldClose(glfwWindow)) {

        Input::update();    
        
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;  
    
        camera->processKeyboard(deltaTime);

        glClearColor(0.2f,0.3f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        
        

        int width, height;
        glfwGetFramebufferSize(glfwWindow, &width, &height);
        if (camera != nullptr)
        {
            projection = glm::perspective(camera->getFov(),
            (float)width / (float)height,
            0.1f, 100.0f);
        
            view = camera->getViewMatrix();
        }
        

        texture.bind(0);
        

        

        gridLines_Shader.use();
        gridLines_Shader.setMat4("projection", projection);
        gridLines_Shader.setMat4("view",view);


        glm::mat4 gridModel = glm::mat4(1.0f);
        basicShader.setMat4("model", gridModel);
        grid.draw();

        basicShader.use();
        basicShader.setInt("uTexture", 0);
        basicShader.setMat4("projection", projection);
        basicShader.setMat4("view",view);

        Transform fishTransform;
        fishTransform.position = glm::vec3(0.0f,0.0f,0.0f);
        fishTransform.rotation.y = 0.59 * currentFrame;
        fishTransform.scale = glm::vec3(1.0f);
        basicShader.setMat4("model", fishTransform.getModelMatrix());
        boxModel.draw();

        glfwSwapBuffers(glfwWindow);
        glfwPollEvents();
    }

    // cleanup
    grid.destroy();
    gridLines_Shader.destroy();
    boxModel.destroy();
    basicShader.destroy();
    texture.destroy();
    glfwTerminate();
    return 0;
}






