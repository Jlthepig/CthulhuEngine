#include "renderer.h"
#include "components.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"
#include "mesh.h"
#include "shader.h"
#include "shadowMap.h"
#include "log_utils.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace Cthulhu::Rendering
{
    void Renderer::setScene(Cthulhu::Scene::Scene* scene)
    {
        this->scene = scene;
    }

    void Renderer::init(GLFWwindow* window, Scene::Camera* camera, const RenderConfig& config)
    {
        this->config = config; // Store config for later use

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 400"); // Updated to match GL 4.0

        this->camera = camera;
        this->window = window;
        
         int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        

        basicShader.load(config.basicVertPath, config.basicFragPath);
        gridShader.load(config.gridVertPath, config.gridFragPath);

        skybox.load(config.skyboxHDRPath);
        skybox.generateIrradianceMap();
        grid.setupGrid(config.gridSize);

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

        glGenTextures(1, &defaultDataTexture);
        glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenTextures(1, &defaultNormalTexture);
        glBindTexture(GL_TEXTURE_2D, defaultNormalTexture);
        unsigned char flatNormalPixel[4] = { 128, 128, 255, 255 }; // Normal pointing straight up
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, flatNormalPixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        basicShader.use();
        basicShader.setInt("uTexture", 0); // diffuse
        basicShader.setInt("uShadowMap", 1); // directional shadow

        for (int i = 0; i < MAX_POINT_SHADOW_CASTERS; i++)
        {
            basicShader.setInt("uPointShadowMaps[" + std::to_string(i) + "]", 2 + i);
        }
        basicShader.setInt("uMetallicRoughnessTexture", 6);
        basicShader.setInt("uNormalMap", 7);
        basicShader.setInt("uBRDFLUT", 8); // Setup slot for BRDF LUT now
        basicShader.setInt("uIrradianceMap", 9);

        shadowMap.init(config.shadowMapResolution, config.shadowMapResolution);
        for (int i = 0; i < MAX_POINT_SHADOW_CASTERS; i++)
        {
            pointShadowMaps[i].init(config.shadowMapResolution, config.shadowMapResolution);
        }
        shadowMap.setLightDir(sunLight.direction);

        // BRDF LUT Generation
        Shader brdfShader;
        brdfShader.load("shaders/brdf.vertex", "shaders/brdf.fragment");
        
        glGenTextures(1, &brdfLUTTexture); 
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, config.brdfLUTSize, config.brdfLUTSize, 0, GL_RG, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        unsigned int brdfFBO;
        glGenFramebuffers(1, &brdfFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, brdfFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

        glViewport(0, 0, config.brdfLUTSize, config.brdfLUTSize);
        brdfShader.use();
        glClear(GL_COLOR_BUFFER_BIT);

        float quadVertices[] = {
            // positions        // texcoords
            -1.0f,  1.0f,      0.0f, 1.0f,
            -1.0f, -1.0f,      0.0f, 0.0f,
             1.0f,  1.0f,      1.0f, 1.0f,
             1.0f, -1.0f,      1.0f, 0.0f,
        };
        unsigned int quadVAO, quadVBO;
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Cleanup BRDF state
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &brdfFBO);
        glDeleteVertexArrays(1, &quadVAO);
        glDeleteBuffers(1, &quadVBO);
        brdfShader.destroy();

        // Restore viewport and shader state
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        basicShader.use(); // Re-bind basic shader so future uniform calls work
    }

    void Renderer::addPointLight(const PointLight& light)
    {
        pointLights.push_back(light);
    }
    
    void Renderer::render(float deltaTime, flecs::world& world)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        totalTriangles = 0;

        if (camera != nullptr)
        {
            projection = glm::perspective(camera->getFov(),
            (float)width / (float)height,
            config.nearPlane, config.farPlane);
            view = camera->getViewMatrix();
        }

        frustum.extractFromMatrix(projection * view);

        // 1. shadow pass
        shadowMap.beginPass();
        world.each([&](flecs::entity e, Scene::TransformComponent& transform, Scene::MeshComponent& mesh) {
            if (!mesh.model) return;
            
            if (transform.matrixDirty) {
                transform.cachedModelMatrix = glm::mat4(1.0f);
                transform.cachedModelMatrix = glm::translate(transform.cachedModelMatrix, transform.position);
                transform.cachedModelMatrix = glm::rotate(transform.cachedModelMatrix, transform.rotation.x, glm::vec3(1,0,0));
                transform.cachedModelMatrix = glm::rotate(transform.cachedModelMatrix, transform.rotation.y, glm::vec3(0,1,0));
                transform.cachedModelMatrix = glm::rotate(transform.cachedModelMatrix, transform.rotation.z, glm::vec3(0,0,1));
                transform.cachedModelMatrix = glm::scale(transform.cachedModelMatrix, transform.scale);
                transform.cachedNormalMatrix = glm::transpose(glm::inverse(transform.cachedModelMatrix));
                transform.matrixDirty = false;
            }

            shadowMap.getDepthShader().setMat4("model", transform.cachedModelMatrix);
            mesh.model->draw();
        });
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

            pointShadowMaps[i].beginPass(lightPos, config.nearPlane, config.farPlane);

            for (int face = 0; face < 6; face++)
            {
                pointShadowMaps[i].bindFace(face, captureViews[face]);
                world.each([&](flecs::entity e, Scene::TransformComponent& transform, Scene::MeshComponent& mesh) {
                    if (!mesh.model) return;
                    pointShadowMaps[i].getDepthShader().setMat4("model", transform.cachedModelMatrix);
                    mesh.model->draw();
                });
            }
            pointShadowMaps[i].endPass();
        }

        // 2. main pass
        glViewport(0, 0, width, height);
        glClearColor(config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        basicShader.use();
        basicShader.setInt("uPointShadowCount", shadowCasters);
        basicShader.setFloat("uPointShadowFarPlane", config.farPlane);
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

                // Bind IBL textures
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getIrradianceMap());

        // reset to slot 0 before entity loop
        glActiveTexture(GL_TEXTURE0);

        int entityCount = 0;
        world.each([&](flecs::entity e, Scene::TransformComponent& transform, Scene::MeshComponent& mesh) {
            if (!e.has<Scene::TagActive>() || !mesh.model) return;
            
            // AABB test
            Scene::AABB worldBounds = TransformAABB({mesh.boundsMin, mesh.boundsMax}, transform.cachedModelMatrix);
            if (!frustum.testAABB(worldBounds)) return;
            
            entityCount++;

            basicShader.setMat4("model", transform.cachedModelMatrix);
            basicShader.setMat4("uNormalMatrix", transform.cachedNormalMatrix);

            for (size_t meshIdx = 0; meshIdx < mesh.model->meshes.size(); meshIdx++)
            {
                auto& modelMesh = mesh.model->meshes[meshIdx];
                glm::vec4 baseColorFactor(1.0f);

                if (modelMesh.materialIndex >= 0 && modelMesh.materialIndex < static_cast<int>(mesh.model->materials.size()))
                {
                    auto& material = mesh.model->materials[modelMesh.materialIndex];
                    baseColorFactor = material.baseColorFactor;

                    basicShader.setFloat("uMetallicFactor", material.metallicFactor);
                    basicShader.setFloat("uRoughnessFactor", material.roughnessFactor);

                    if (material.baseColorTextureIndex >= 0 &&
                        material.baseColorTextureIndex < static_cast<int>(mesh.model->textures.size()))
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, mesh.model->textures[material.baseColorTextureIndex].getID());
                    }

                    if (material.metallicRoughnessTextureIndex >= 0 &&
                        material.metallicRoughnessTextureIndex < static_cast<int>(mesh.model->textures.size()))
                    {
                        glActiveTexture(GL_TEXTURE6);
                        glBindTexture(GL_TEXTURE_2D, mesh.model->textures[material.metallicRoughnessTextureIndex].getID());
                    }
                    else
                    {
                        glActiveTexture(GL_TEXTURE6);
                        glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
                    }
                    
                    if (material.normalTextureIndex >= 0 &&
                        material.normalTextureIndex < static_cast<int>(mesh.model->textures.size()))
                    {
                        glActiveTexture(GL_TEXTURE7);
                        glBindTexture(GL_TEXTURE_2D, mesh.model->textures[material.normalTextureIndex].getID());
                    }
                    else
                    {
                        glActiveTexture(GL_TEXTURE7);
                        glBindTexture(GL_TEXTURE_2D, defaultNormalTexture);
                    }
                }
                else
                {
                    basicShader.setFloat("uMetallicFactor", 0.0f);
                    basicShader.setFloat("uRoughnessFactor", 1.0f);
                    glActiveTexture(GL_TEXTURE6);
                    glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
                    glActiveTexture(GL_TEXTURE7);
                    glBindTexture(GL_TEXTURE_2D, defaultNormalTexture);
                }

                basicShader.setVec4("uBaseColorFactor", baseColorFactor);
                totalTriangles += modelMesh.getIndexCount() / 3;
                modelMesh.draw();
            }
        });

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
        ImGui::Text("Shadow Map Resolution: %d", config.shadowMapResolution);
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