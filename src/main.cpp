

#define STB_IMAGE_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "stb_image.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include <iostream>
#include <algorithm>
#include "shader.h"


using Cthulhu::Rendering::Shader;

//Window settings
const int SCR_WIDTH = 1920;
const int SCR_HEIGHT = 1920;

// time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// mouse
bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH/2.0;
float lastY = SCR_HEIGHT/2.0;
float fov = 45.0f;



float vertices[] = {
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


unsigned int indices[] = {
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


int width = 128;
int height = 128;
int nrChannels;


// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void setupCube(unsigned int &VAO, unsigned int &VBO,unsigned int &EBO);
void cleanup(unsigned int &VAO,unsigned int &VBO,unsigned int &EBO);
void mouse_callback(GLFWwindow* window,double xposIn, double yposIn);


glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

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

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    Shader shader;


    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glm::mat4 view;
    
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);
    glEnable(GL_DEPTH_TEST);


    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load("./assets/images/lava.png",  &width,  &height,  &nrChannels,0);

    if (!data)
    {
        std::cout << "Image data missing" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    
    shader.load("shaders/cube.vertex","shaders/cube.fragment");

  
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;

    unsigned int texture;
    glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Use mipmaps if generated
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = GL_RGB;
    if (nrChannels == 4) format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D); 

    

    setupCube(VAO, VBO,EBO);

    // Main Loop
    while (!glfwWindowShouldClose(window)) {

            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;  
    
        processInput(window);

        glClearColor(0.2f,0.3f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        glActiveTexture(GL_TEXTURE0); // Activate texture unit 0
        glBindTexture(GL_TEXTURE_2D, texture);
        shader.use();
        shader.setInt("uTexture", 0);
        
        glm::mat4 model = glm::mat4(1.0f); 
        float time = (float)glfwGetTime();
        model = glm::rotate(model, time * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));

        shader.setMat4("model", model);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        projection = glm::perspective(glm::radians(90.0f),
            (float)width / (float)height,
            0.1f, 100.0f);

        shader.setMat4("projection", projection);
        
        view = glm::lookAt(cameraPos,cameraPos+cameraFront,cameraUp);
        shader.setMat4("view", view);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT,0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanup(VAO,VBO,EBO);
    shader.destroy();
    glDeleteTextures(1,&texture);
    stbi_image_free(data);
    glfwTerminate();
    return 0;
}



void setupCube(unsigned int &VAO, unsigned int &VBO,unsigned int &EBO)
{
    
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1,&EBO);

    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5 * sizeof(float), (void*) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glBindVertexArray(0); 
    

}
void cleanup(unsigned int &VAO,unsigned int &VBO,unsigned int &EBO) 
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    
    
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

     float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;

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

    float sensitivity = 0.1f; // change this value to your liking
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;
    pitch = std::clamp(pitch,-89.0f,89.0f); 

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

}