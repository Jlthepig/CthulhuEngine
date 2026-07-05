#include "pch.h"
#include "model.h"
#include "renderer.h"
#include "components.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "fwd.hpp"
#include "mesh.h"
#include "shader.h"
#include "shadowMap.h"
#include "log_utils.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
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
        ImGui_ImplOpenGL3_Init("#version 430"); // Updated to match GL 4.3

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
        skybox.generatePrefilterMap();
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
        basicShader.setInt("uPrefilterMap", 10);

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

        lineShader.load("shaders/debugLine.vertex", "shaders/debugLine.fragment");
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DebugVertex) * 1000, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)(sizeof(glm::vec3)));
        
        glBindVertexArray(0);

        // Restore viewport and shader state
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        basicShader.use(); // Re-bind basic shader so future uniform calls work

        glGenBuffers(1, &sceneUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, sceneUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, sceneUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        Log::Print("Renderer Initialized Successfully", "ENGINE", LogType::LOG_SUCCESS);
    }

    void Renderer::addPointLight(const PointLight& light)
    {
        pointLights.push_back(light);
    }
    
    void Renderer::render(float fps, float deltaTime, const std::vector<Renderable>& renderables)
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
        for (const auto& renderable : renderables)
        {
            shadowMap.getDepthShader().setMat4("model", renderable.modelMatrix);
            renderable.model->draw();
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

            pointShadowMaps[i].beginPass(lightPos, config.nearPlane, config.farPlane);

            for (int face = 0; face < 6; face++)
            {
                pointShadowMaps[i].bindFace(face, captureViews[face]);
                for (const auto& renderable : renderables)
                {
                    pointShadowMaps[i].getDepthShader().setMat4("model", renderable.modelMatrix);
                    renderable.model->draw();
                }
            }
            pointShadowMaps[i].endPass();
        }
        // 2. main pass
        glViewport(0, 0, width, height);
        glClearColor(config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        basicShader.use();

        SceneUniforms uboData;
        uboData.view = view;
        uboData.projection = projection;
        uboData.viewPos = camera->getPosition();

        uboData.lightDir = sunLight.direction;
        uboData.lightColor = sunLight.color;
        uboData.lightIntensity = sunLight.intensity;

        uboData.fogColor = config.fogColor;
        uboData.fogDensity = config.fogDensity;
        uboData.fogHeightFalloff = config.fogHeightFalloff;

        uboData.pointLightCount = (int)pointLights.size();
        uboData.pointShadowCount = shadowCasters;
        uboData.pointShadowfarPlane = config.farPlane;

        for (int i = 0; i < uboData.pointLightCount; i++)
        {
            uboData.pointLights[i].position = pointLights[i].position;
            uboData.pointLights[i].color = pointLights[i].color;
            uboData.pointLights[i].intensity = pointLights[i].intensity;
            uboData.pointLights[i].constant = pointLights[i].constant;
            uboData.pointLights[i].linear = pointLights[i].linear;
            uboData.pointLights[i].quadratic = pointLights[i].quadratic;
        }
        // upload to gpu in one call
        glBindBuffer(GL_UNIFORM_BUFFER, sceneUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneUniforms), &uboData);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
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

        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getPrefilterMap());

        // reset to slot 0 before entity loop
        glActiveTexture(GL_TEXTURE0);

        int entityCount = 0;
        for (const auto& renderable : renderables)        
        {   
            // AABB test
            Scene::AABB worldBounds = TransformAABB({renderable.boundsMin, renderable.boundsMax}, renderable.modelMatrix);
            if (!frustum.testAABB(worldBounds.min,worldBounds.max)) continue;
            
            entityCount++;

            basicShader.setMat4("model", renderable.modelMatrix);
            basicShader.setMat4("uNormalMatrix", renderable.normalMatrix);

            for (size_t meshIdx = 0; meshIdx < renderable.model->meshes.size(); meshIdx++)
            {
                auto& modelMesh = renderable.model->meshes[meshIdx];
                glm::vec4 baseColorFactor(1.0f);

                if (modelMesh.materialIndex >= 0 && modelMesh.materialIndex < static_cast<int>(renderable.model->materials.size()))
                {
                    auto& material = renderable.model->materials[modelMesh.materialIndex];
                    baseColorFactor = material.baseColorFactor;

                    basicShader.setFloat("uMetallicFactor", material.metallicFactor);
                    basicShader.setFloat("uRoughnessFactor", material.roughnessFactor);

                    if (material.baseColorTextureIndex >= 0 &&
                        material.baseColorTextureIndex < static_cast<int>(renderable.model->textures.size()))
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, renderable.model->textures[material.baseColorTextureIndex].getID());
                    }

                    if (material.metallicRoughnessTextureIndex >= 0 &&
                        material.metallicRoughnessTextureIndex < static_cast<int>(renderable.model->textures.size()))
                    {
                        glActiveTexture(GL_TEXTURE6);
                        glBindTexture(GL_TEXTURE_2D, renderable.model->textures[material.metallicRoughnessTextureIndex].getID());
                    }
                    else
                    {
                        glActiveTexture(GL_TEXTURE6);
                        glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
                    }
                    
                    if (material.normalTextureIndex >= 0 &&
                        material.normalTextureIndex < static_cast<int>(renderable.model->textures.size()))
                    {
                        glActiveTexture(GL_TEXTURE7);
                        glBindTexture(GL_TEXTURE_2D, renderable.model->textures[material.normalTextureIndex].getID());
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
        };

        // 3. grid
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gridShader.use();
        glm::mat4 gridModel = glm::mat4(1.0f);
        gridShader.setMat4("model", gridModel);
        grid.draw();
        glDisable(GL_BLEND);

        // 4. skybox
        skybox.draw(window, view, projection);

        for (size_t i = 0; i < persistentLines.size(); ) 
        {
            persistentLines[i].lifetime -= deltaTime;
            
            if (persistentLines[i].lifetime <= 0.0f) 
            {
                // Line is dead. Swap it with the last element in the array and pop it.
                // This avoids the massive performance cost of shifting array elements!
                persistentLines[i] = persistentLines.back();
                persistentLines.pop_back();
                // Note: We DO NOT increment 'i' here, because we need to check 
                // the new line that was just swapped into this index.
            } 
            else 
            {
                // Line is still alive. Push it to the immediate draw buffer for this frame.
                debugLines.push_back({persistentLines[i].start, persistentLines[i].color});
                debugLines.push_back({persistentLines[i].end, persistentLines[i].color});
                i++;
            }
        }

        if(!debugLines.empty())
        {
            glDisable(GL_DEPTH_TEST);
            lineShader.use();
            lineShader.setMat4("projection",projection);
            lineShader.setMat4("view", view);

            glBindVertexArray(lineVAO);
            glBindBuffer(GL_ARRAY_BUFFER,lineVBO);
            glBufferData(GL_ARRAY_BUFFER, debugLines.size() * sizeof(DebugVertex), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, debugLines.size() * sizeof(DebugVertex), debugLines.data());

            glLineWidth(3.0f);
            glDrawArrays(GL_LINES,0,debugLines.size());
            glLineWidth(1.0f);

            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
            debugLines.clear();
        }

        // 5. imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Entities: %d", entityCount);
        ImGui::Text("Draw Calls: %d", entityCount + 2);  // +1 grid +1 skybox
        ImGui::Text("Triangles: %zu", totalTriangles);
        ImGui::Text("Shadow Map Resolution: %d", config.shadowMapResolution);

        ImGui::Separator();
        ImGui::Text("Exponential Height Fog");
        ImGui::ColorEdit3("Fog Color", glm::value_ptr(config.fogColor));
        ImGui::SliderFloat("Density", &config.fogDensity, 0.0f, 0.1f, "%.4f");
        ImGui::SliderFloat("Height Falloff", &config.fogHeightFalloff, 0.0f, 1.0f, "%.3f");

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
        Log::Print("Renderer shutdown successfully", "ENGINE", LogType::LOG_SUCCESS);
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

    void Renderer::addDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color,float duration) 
    {
            if (duration <= 0.0f) {
            // (disappears next frame)
            debugLines.push_back({start, color});
            debugLines.push_back({end, color});
        } else {
            // (stays on screen)
            persistentLines.push_back({start, end, color, duration});
        }
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