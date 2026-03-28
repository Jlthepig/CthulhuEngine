
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "stb_image.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include <iostream>
#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "texture.h"

using Cthulhu::Rendering::Shader;
using Cthulhu::Scene::Camera;
using Cthulhu::Rendering::Mesh;
using Cthulhu::Rendering::Texture;

//Window settings
const int SCR_WIDTH = 1920;
const int SCR_HEIGHT = 1920;

// time
double deltaTime = 0.0f;
double lastFrame = 0.0f;

// mouse
bool firstMouse = true;
float lastX = SCR_WIDTH/2.0;
float lastY = SCR_HEIGHT/2.0;

// instantiate objects
Camera camera;
Mesh mesh;
Texture texture;

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

// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window,double xposIn, double yposIn);
void mouse_button_callback(GLFWwindow* window,int button,int action,int mods);

int main(void)
{
    // Initialize
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GLFW Window", NULL, NULL);
    if (!window) {  std::cout << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    Shader shader;
    

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    camera.init();
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    glm::mat4 projection = glm::perspective(camera.getFov(), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);
    glEnable(GL_DEPTH_TEST);


    texture.load("./assets/images/lava.png");
    shader.load("shaders/cube.vertex","shaders/cube.fragment");
    mesh.setup(vertices, indices);
    

    // Main Loop
    while (!glfwWindowShouldClose(window)) {

        double currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;  
    
        camera.processKeyboard(window, deltaTime);

        glClearColor(0.2f,0.3f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        texture.bind(0);
        shader.use();
        shader.setInt("uTexture", 0);
        
        glm::mat4 model = glm::mat4(1.0f); 
        float time = (float)glfwGetTime();
        model = glm::rotate(model, time * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));

        shader.setMat4("model", model);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        projection = glm::perspective(camera.getFov(),
            (float)width / (float)height,
            0.1f, 100.0f);

        shader.setMat4("projection", projection);
        
        glm::mat4 view = camera.getViewMatrix();
        shader.setMat4("view", view);

        
        for (auto& pos : cubePositions)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pos);
            shader.setMat4("model", model);
            mesh.draw();
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    mesh.destroy();
    shader.destroy();
    texture.destroy();
    glfwTerminate();
    return 0;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;

    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

   camera.processMouse(xoffset,yoffset);

}

void mouse_button_callback(GLFWwindow* window,int button,int action,int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}