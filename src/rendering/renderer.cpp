#include "renderer.h"
#include "entity.h"
#include "ext/matrix_clip_space.hpp"
#include "shader.h"
#include "shadowMap.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

const float near = 0.1f;
const float far = 100.0f;
const float gridSize = 256.0f;
const float shadowMapResolution = 2048.0f;
const glm::vec4 fogColor(0.2f, 0.3f, 0.3f, 1.0f);
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
        this->projection = glm::perspective(camera->getFov(), (float)width / (float)height, near, far);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        

        basicShader.load("shaders/basic.vertex","shaders/basic.fragment");
        gridShader.load("shaders/grid.vertex", "shaders/grid.fragment");

       
        skybox.load("assets/images/hdriTest.hdr");
        grid.setupGrid(gridSize);

        shadowMap.init(shadowMapResolution, shadowMapResolution);
        shadowMap.setLightDir(sunLight.direction);

    }

    void Renderer::addPointLight(const PointLight& light)
    {
        pointLights.push_back(light);
    }
    
    void Renderer::render(float deltaTime)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        totalTriangles = 0;

        if (camera != nullptr)
        {
            projection = glm::perspective(camera->getFov(),
            (float)width / (float)height,
            near, far);
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
        glClearColor(fogColor.r, fogColor.g, fogColor.b, fogColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        basicShader.use();
        basicShader.setVec3("uLightDir", sunLight.direction);
        basicShader.setVec3("uLightColor", sunLight.color);
        basicShader.setFloat("uLightIntensity", sunLight.intensity);
        basicShader.setInt("uPointLightCount", (int)pointLights.size());
        for (int i = 0; i < (int)pointLights.size(); i++)
        {
            std::string base = "uPointLights[" + std::to_string(i) + "].";
            basicShader.setVec3(base + "position", pointLights[i].position);
            basicShader.setVec3(base + "color", pointLights[i].color);
            basicShader.setFloat(base + "intensity", pointLights[i].intensity);
            basicShader.setFloat(base + "constant", pointLights[i].constant);
            basicShader.setFloat(base + "linear", pointLights[i].linear);
            basicShader.setFloat(base + "quadratic", pointLights[i].quadratic);
        }
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

            // Draw each mesh individually with its own material
            for (size_t meshIdx = 0; meshIdx < entity.model->meshes.size(); meshIdx++)
            {
                auto& mesh = entity.model->meshes[meshIdx];
                // Get a material 
                glm::vec4 baseColorFactor(1.0f); 
                if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int>(entity.model->materials.size()))
                {
                    auto& material = entity.model->materials[mesh.materialIndex];
                    baseColorFactor = material.baseColorFactor;
                    
                    // Bind a texture if can
                    if (material.baseColorTextureIndex >= 0 && 
                        material.baseColorTextureIndex < static_cast<int>(entity.model->textures.size()))
                    {
                        entity.model->textures[material.baseColorTextureIndex].bind(0);
                    }
                }

                basicShader.setVec4("uBaseColorFactor", baseColorFactor);

                for (auto& mesh : entity.model->meshes)
                {
                    totalTriangles += mesh.getIndexCount() / 3;
                }
                mesh.draw();
            }
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
        ImGui::Text("Draw Calls: %d", entityCount + 2);  // +1 grid +1 skybox
        ImGui::Text("Triangles: %zu", totalTriangles);
        ImGui::Text("Shadow Map Resolution: %d", static_cast<int>(shadowMapResolution));
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

    void Renderer::setDirectionalLight(const DirectionalLight& light)
    {
        sunLight = light;
        shadowMap.setLightDir(light.direction);
    }

    void Renderer::setPointLights(const std::vector<PointLight>& lights)
    {
        pointLights = lights;
    }
    
}