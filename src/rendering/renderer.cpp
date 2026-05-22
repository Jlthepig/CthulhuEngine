#include "renderer.h"
#include "entity.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"
#include "shader.h"
#include "shadowMap.h"
#include "log_utils.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
namespace
{
    // Rendering configuration
    constexpr float CAMERA_NEAR_PLANE = 0.1f;
    constexpr float CAMERA_FAR_PLANE = 100.0f;
    constexpr float GRID_SIZE = 256.0f;
    constexpr float SHADOW_MAP_RESOLUTION = 2048.0f;
    constexpr glm::vec4 FOG_COLOR(0.2f, 0.3f, 0.3f, 1.0f);

    // Shader paths
    constexpr const char* BASIC_VERTEX_SHADER = "shaders/basic.vertex";
    constexpr const char* BASIC_FRAGMENT_SHADER = "shaders/basic.fragment";
    constexpr const char* GRID_VERTEX_SHADER = "shaders/grid.vertex";
    constexpr const char* GRID_FRAGMENT_SHADER = "shaders/grid.fragment";
    constexpr const char* SKYBOX_HDR_PATH = "assets/images/hdriTest.hdr";
    constexpr const char* IMGUI_GLSL_VERSION = "#version 330";

    // Texture slots
    constexpr int DIFFUSE_TEXTURE_SLOT = 0;
    constexpr int SHADOW_MAP_TEXTURE_SLOT = 1;

    // Debug UI
    constexpr int ADDITIONAL_DRAW_CALLS = 2; // grid + skybox
    constexpr int TRIANGLES_PER_FACE = 3;
    constexpr int AABB_CORNER_COUNT = 8;
}
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
        ImGui_ImplOpenGL3_Init(IMGUI_GLSL_VERSION);

        this->camera = camera;
        this->window = window;
        
         int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        

        basicShader.load(BASIC_VERTEX_SHADER,BASIC_FRAGMENT_SHADER);
        basicShader.use();
        basicShader.setInt("uTexture", 0); // diffuse
        basicShader.setInt("uShadowMap", 1); // directional shadow

        for (int i = 0; i < MAX_POINT_SHADOW_CASTERS; i++)
        {
            basicShader.setInt("uPointShadowMaps[" + std::to_string(i) + "]", 2 + i);
        }
        gridShader.load(GRID_VERTEX_SHADER, GRID_FRAGMENT_SHADER);

       
        skybox.load(SKYBOX_HDR_PATH);
        grid.setupGrid(GRID_SIZE);

        // temp setup for whitepointshadow
        glGenTextures(1, &whitePointShadow);
        glBindTexture(GL_TEXTURE_CUBE_MAP, whitePointShadow);

        float whiteDepth = 1.0f; // far plane = "no shadow"
        for (int face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
                        GL_DEPTH_COMPONENT24, 1, 1, 0,
                        GL_DEPTH_COMPONENT, GL_FLOAT, &whiteDepth);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        KalaHeaders::KalaLog::Log::Print("White point shadow fallback created", "Renderer", KalaHeaders::KalaLog::LogType::LOG_SUCCESS);

        shadowMap.init(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION);
        for (int i = 0; i < MAX_POINT_SHADOW_CASTERS; i++)
        {
            pointShadowMaps[i].init(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION);
        }
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
            CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
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
        int shadowCasters = std::min((int)pointLights.size(), MAX_POINT_SHADOW_CASTERS);

        for (int i = 0; i < shadowCasters; i++)
        {
            glm::vec3 lightPos = pointLights[i].position;
    
                glm::mat4 captureViews[] = {
                    glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f,-1.0f, 0.0f)),
                    glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f,-1.0f, 0.0f)),
                    glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                    glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.0f, 0.0f,-1.0f)),
                    glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f,-1.0f, 0.0f)),
                    glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.0f,-1.0f, 0.0f))
                };

            pointShadowMaps[i].beginPass(lightPos, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

            for (int face = 0; face < 6; face++)
            {
                pointShadowMaps[i].bindFace(face, captureViews[face]);
                for (auto& entity : scene->getEntities())
                {
                    if (!entity.active || entity.model == nullptr) continue;
                    glm::mat4 modelMatrix = entity.transform.getModelMatrix(); // once per entity
                    pointShadowMaps[i].getDepthShader().setMat4("model", modelMatrix);
                    entity.model->draw();
                }
            }
            pointShadowMaps[i].endPass();
        }

        // 2. main pass
        glViewport(0, 0, width, height);
        glClearColor(FOG_COLOR.r, FOG_COLOR.g, FOG_COLOR.b, FOG_COLOR.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        basicShader.use();
        basicShader.setInt("uPointShadowCount", shadowCasters);
        basicShader.setFloat("uPointShadowFarPlane", CAMERA_FAR_PLANE);
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
        basicShader.setMat4("projection", projection);
        basicShader.setMat4("view", view);
        basicShader.setMat4("lightSpaceMatrix", shadowMap.getLightSpaceMatrix());

        // bind all textures in order: 0=diffuse(per mesh), 1=shadow, 2+=cubemaps
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowMap.getDepthMap());

        for (int i = 0; i < MAX_POINT_SHADOW_CASTERS; i++)
        {
            glActiveTexture(GL_TEXTURE2 + i);
            if (i < shadowCasters) {
                glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowMaps[i].getDepthCubeMap());
            } else {
                glBindTexture(GL_TEXTURE_CUBE_MAP, whitePointShadow); // 1x1 depth=1.0 = no shadow
            }
        }

        // reset to slot 0 before entity loop
        glActiveTexture(GL_TEXTURE0);

        int entityCount = 0;
        for (auto& entity : scene->getEntities())
        {
            if (!entity.active || entity.model == nullptr) continue;
            glm::mat4 modelMatrix = entity.transform.getModelMatrix();
            Scene::AABB worldBounds = TransformAABB(entity.bounds, modelMatrix);
            if (!frustum.testAABB(worldBounds)) continue;
            entityCount++;

            basicShader.setMat4("model", modelMatrix);

            for (size_t meshIdx = 0; meshIdx < entity.model->meshes.size(); meshIdx++)
            {
                auto& mesh = entity.model->meshes[meshIdx];
                glm::vec4 baseColorFactor(1.0f);

                if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int>(entity.model->materials.size()))
                {
                    auto& material = entity.model->materials[mesh.materialIndex];
                    baseColorFactor = material.baseColorFactor;

                    if (material.baseColorTextureIndex >= 0 &&
                        material.baseColorTextureIndex < static_cast<int>(entity.model->textures.size()))
                    {
                        // explicitly bind to slot 0 without disturbing other slots
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, entity.model->textures[material.baseColorTextureIndex].getID());
                    }
                }

                basicShader.setVec4("uBaseColorFactor", baseColorFactor);
                totalTriangles += mesh.getIndexCount() / TRIANGLES_PER_FACE;
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
        ImGui::Text("Draw Calls: %d", entityCount + ADDITIONAL_DRAW_CALLS);  // +1 grid +1 skybox
        ImGui::Text("Triangles: %zu", totalTriangles);
        ImGui::Text("Shadow Map Resolution: %d", static_cast<int>(SHADOW_MAP_RESOLUTION));
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

            for (int i = 0; i < AABB_CORNER_COUNT; i++)
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