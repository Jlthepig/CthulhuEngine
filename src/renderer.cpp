#include "renderer.h"
#include "ext/matrix_clip_space.hpp"
#include "modelLoader.h"
#include "shader.h"

namespace Cthulhu::Rendering
{
    void Renderer::init(GLFWwindow* window, Scene::Camera* camera)
    {

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        this->camera = camera;
        this->window = window;
        
         int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        this->projection = glm::perspective(camera->getFov(), (float)width / (float)height, 0.1f, 100.0f);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        basicShader.load("shaders/basic.vertex","shaders/basic.fragment");
        gridShader.load("shaders/grid.vertex", "shaders/grid.fragment");

        fishModel = ModelLoader::loadGltf("assets/models/BarramundiFish.glb");
        skybox.load("assets/images/hdriTest.hdr");
        grid.setupGrid(256);

        fishTransform.position = glm::vec3(0.0f);
        fishTransform.scale = glm::vec3(1.0f);
    }
    
    void Renderer::render(float deltaTime)
    {
        glClearColor(0.2f,0.3f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::End();

        float currentFrame = glfwGetTime(); 

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (camera != nullptr)
        {
            projection = glm::perspective(camera->getFov(),
            (float)width / (float)height,
            0.1f, 100.0f);
        
            view = camera->getViewMatrix();
        }

        basicShader.use();
        basicShader.setVec3("uLightDir", sunLight.direction);
        basicShader.setVec3("uLightColor", sunLight.color);
        basicShader.setFloat("uLightIntensity", sunLight.intensity);
        basicShader.setVec3("uViewPos", camera->getPosition());
        basicShader.setInt("uTexture", 0);
        basicShader.setMat4("projection", projection);
        basicShader.setMat4("view",view);

        fishTransform.rotation.y = 0.59 * currentFrame;
        basicShader.setMat4("model", fishTransform.getModelMatrix());
        fishModel.draw();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        gridShader.use();
        gridShader.setVec3("cameraPos", camera->getPosition());
        gridShader.setMat4("projection", projection);
        gridShader.setMat4("view", view);
        glm::mat4 gridModel = glm::mat4(1.0f);
        gridShader.setMat4("model", gridModel);
        grid.draw();

        glDisable(GL_BLEND);

        skybox.draw(window, view, projection);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    
    void Renderer::shutdown()
    {
        grid.destroy();
        gridShader.destroy();
        fishModel.destroy();
        basicShader.destroy();
        skybox.destroy();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    
}