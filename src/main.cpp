
// standard libraries
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

// engine types
using Cthulhu::Rendering::Shader;
using Cthulhu::Scene::Camera;
using Cthulhu::Rendering::Mesh;
using Cthulhu::Rendering::Texture;
using Cthulhu::Rendering::Model;
using Cthulhu::Rendering::ModelLoader;
using Cthulhu::Core::Window;

// utilities
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// window settings
glm::vec2 resolution = glm::vec2(1920.0f,1080.0f);

// frame state
double deltaTime = 0.0f;
double lastFrame = 0.0f;



// test scene data


std::vector<float> vertices = {
    // Positions            // Texture Coords
    // Back face
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, // 0
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // 1
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // 2
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, // 3

    // Front face
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // 4
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, // 5
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, // 6
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, // 7

    // Left face
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // 8
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, // 9
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // 10
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, // 11

    // Right face
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // 12
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // 13
     0.5f,  0.5f,  0.5f,  0.0f, 1.0f, // 14
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // 15

    // Bottom face
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, // 16
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f, // 17
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, // 18
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // 19

    // Top face
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // 20
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, // 21
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, // 22
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f  // 23
};
std::vector<unsigned int> indices = {
    0,  3,  2,
               2,  1,  0,
               4,  5,  6,
               6,  7,  4,
               11, 8,  9,
               9,  10, 11,
               12, 13, 14,
               14, 15, 12,
               16, 17, 18,
               18, 19, 16,
               20, 21, 22,
               22, 23, 20
};
std::vector<glm::vec3> cubePositions = {
    glm::vec3( 0.0f,  0.0f,  0.0f),
    glm::vec3( 2.0f,  0.0f, -2.0f),
    glm::vec3(-2.0f,  0.0f, -4.0f),
};
std::vector<Mesh::vertexAttribute> attrs = {
{0,3,0},
    {1,2,3 * sizeof(float)}


};



int main()
{
    // Initialize system
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window* window = Cthulhu::Core::Window::createWindow(resolution,"Cthulhu Engine");
    GLFWwindow* glfwWindow = window->getWindow();
    Camera* camera = Camera::init();
    window->setCamera(camera);
    Log::Print("start log", "Main", LogType::LOG_INFO);
    Mesh mesh;
    Texture texture;

    // Window creation
    

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
         Log::Print("FAILED TO INITIALISE GLAD.", "Main", LogType::LOG_ERROR);
        return -1;
    }

    // loading resources
    Shader shader;
    Shader basicShader;
    Model boxModel = ModelLoader::loadGltf("assets/models/Box.glb");
    texture.load("./assets/images/lava.png");
    shader.load("shaders/cube.vertex","shaders/cube.fragment");
    basicShader.load("shaders/basic.vertex","shaders/basic.fragment");
    mesh.setup(vertices, indices,attrs,5 * sizeof(float));

    int fbW, fbH;
    if (glfwWindow == NULL)
    {
        Log::Print("WINDOW IS NULL", "Main", LogType::LOG_ERROR);
        exit(1);
    }
    glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // scene setup
    
    glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glm::mat4 projection = glm::perspective(camera->getFov(), (float)window->getWidth()/(float)window->getHeight(), 0.1f, 100.0f);
    glm::mat4 view{};
    glEnable(GL_DEPTH_TEST);

    // Main Loop
    while (!glfwWindowShouldClose(glfwWindow)) {

        double currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;  
    
        camera->processKeyboard(glfwWindow, deltaTime);

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
        shader.use();
        shader.setInt("uTexture", 0);
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        for (auto& pos : cubePositions)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pos);
            shader.setMat4("model", model);
            mesh.draw();
        }

        basicShader.use();
        basicShader.setMat4("projection", projection);
        basicShader.setMat4("view",view);


        glm::mat4 boxModelMatrix = glm::mat4(1.0f);
        boxModelMatrix = glm::translate(boxModelMatrix, glm::vec3(5.0f, 0.0f, 0.0f));
        basicShader.setMat4("model", boxModelMatrix);
        boxModel.draw();

        glfwSwapBuffers(glfwWindow);
        glfwPollEvents();
    }

    // cleanup
    mesh.destroy();
    boxModel.destroy();
    shader.destroy();
    basicShader.destroy();
    texture.destroy();
    glfwTerminate();
    return 0;
}






