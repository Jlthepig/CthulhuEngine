#include "renderer.h"
#include "entity.h"
#include "ext/matrix_clip_space.hpp"
#include "modelLoader.h"
#include "shader.h"
#include "shadowMap.h"

namespace Cthulhu::Rendering
{
    void Renderer::setScene(Cthulhu::Scene::Scene* scene)
    {
        this->scene = scene;
    }

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

       
        skybox.load("assets/images/suburban_garden_1k.hdr");
        grid.setupGrid(256);

        

        shadowMap.init(2048, 2048);
        shadowMap.setLightDir(sunLight.direction);

    }
    
    void Renderer::render(float deltaTime)
    {
        
        
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

        frustum.extractFromMatrix(projection * view);

        // 1. shadow pass
        shadowMap.beginPass();
        for (auto& entity : scene->getEntities())
        {
            if (!entity.active || entity.model == nullptr) continue;
            glm::mat4 modelMatrix = entity.transform.getModelMatrix(); // once per entity
            shadowMap.getDepthShader().setMat4("model", modelMatrix);
            entity.model->draw();
        }
        shadowMap.endPass();
        

        // 2. main pass
        glViewport(0, 0, width, height);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        basicShader.use();
        basicShader.setVec3("uLightDir", sunLight.direction);
        basicShader.setVec3("uLightColor", sunLight.color);
        basicShader.setFloat("uLightIntensity", sunLight.intensity);
        basicShader.setVec3("uViewPos", camera->getPosition());
        basicShader.setInt("uTexture", 0);
        basicShader.setInt("uShadowMap", 1);
        basicShader.setMat4("projection", projection);
        basicShader.setMat4("view", view);
        basicShader.setMat4("lightSpaceMatrix", shadowMap.getLightSpaceMatrix());
        

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowMap.getDepthMap());

        int entityCount = 0;
        for (auto& entity : scene->getEntities())
        {
            if (!entity.active || entity.model == nullptr) continue;
            glm::mat4 modelMatrix = entity.transform.getModelMatrix(); 
            Scene::AABB worldBounds = TransformAABB(entity.bounds, modelMatrix);
            
            if (!frustum.testAABB(worldBounds)) continue;
            entityCount++;

            basicShader.setMat4("model", modelMatrix);
            entity.model->draw();
        }

        

        // 3. grid
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

        // 4. skybox
        skybox.draw(window, view, projection);

        // 5. imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::Text("Entities: %d", entityCount);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    
    void Renderer::shutdown()
    {
        grid.destroy();
        gridShader.destroy();
        basicShader.destroy();
        skybox.destroy();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    
    Scene::AABB Renderer::TransformAABB(const Scene::AABB& localBounds, const glm::mat4& modelMatrix)
    {
            // get all 8 corners of the local AABB
            glm::vec3 corners[8] = {
                glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.min.z),
                glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.min.z),
                glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.min.z),
                glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.min.z),
                glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.max.z),
                glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.max.z),
                glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.max.z),
                glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.max.z)
            };

            // transform each corner to world space and compute new min/max
            glm::vec3 worldMin = glm::vec3(FLT_MAX);
            glm::vec3 worldMax = glm::vec3(-FLT_MAX);

            for (int i = 0; i < 8; i++)
            {
                glm::vec4 worldCorner = modelMatrix * glm::vec4(corners[i], 1.0f);
                worldMin = glm::min(worldMin, glm::vec3(worldCorner));
                worldMax = glm::max(worldMax, glm::vec3(worldCorner));
            }

            Scene::AABB worldBounds{ worldMin, worldMax };
            return worldBounds;
    }

    
}