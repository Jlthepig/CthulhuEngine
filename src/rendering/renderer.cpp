#include <algorithm>
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>
#include <limits>
#include <type_traits>

#include "components.hpp"
#include "log_utils.hpp"
#include "mesh.hpp"
#include "model.hpp"
#include "renderer.hpp"
#include "sceneUniforms.hpp"
#include "shader.hpp"
#include "shadowMap.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Rendering
{
static_assert(std::is_standard_layout_v<OverlayVertex>);

void Renderer::setScene(Cthulhu::Scene::Scene *scene)
{
    this->scene = scene;
    outlineTargets.clear();
}

void Renderer::setOutlineTargets(std::span<const Scene::EntityId> entities)
{
    outlineTargets.assign(entities.begin(), entities.end());
}

void Renderer::init(GLFWwindow *window, Scene::Camera *camera, const RenderConfig &config)
{
    this->config = config;

    auto enginePath = [&config](const std::filesystem::path &relative) {
        return (config.engineResourceRoot / relative).lexically_normal().string();
    };

    this->camera = camera;
    this->window = window;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    sceneFramebuffer.create(width, height);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    basicShader.load(enginePath("shaders/basic.vertex"), enginePath("shaders/basic.fragment"));
    gridShader.load(enginePath("shaders/grid.vertex"), enginePath("shaders/grid.fragment"));

    skybox.load(config.engineResourceRoot, config.engineResourceRoot / "images/Test2.hdr");
    skybox.generateIrradianceMap();
    skybox.generatePrefilterMap();
    grid.setupGrid(config.gridSize);

    glGenTextures(1, &defaultDataTexture);
    glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &defaultNormalTexture);
    glBindTexture(GL_TEXTURE_2D, defaultNormalTexture);
    unsigned char flatNormalPixel[4] = {128, 128, 255, 255}; // Normal pointing straight up
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, flatNormalPixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    basicShader.use();
    basicShader.setInt("uTexture", 0);   // diffuse
    basicShader.setInt("uShadowMap", 1); // directional shadow
    basicShader.setInt("uPointShadowMaps", 2);
    basicShader.setInt("uMetallicRoughnessTexture", 6);
    basicShader.setInt("uNormalMap", 7);
    basicShader.setInt("uBRDFLUT", 8); // Setup slot for BRDF LUT now
    basicShader.setInt("uIrradianceMap", 9);
    basicShader.setInt("uPrefilterMap", 10);

    shadowMap.init(config.shadowMapResolution, config.shadowMapResolution, config.engineResourceRoot);
    pointShadowMap.init(config.shadowMapResolution, config.shadowMapResolution, config.engineResourceRoot);
    shadowMap.setLightDir(sunLight.direction);

    // BRDF LUT Generation
    Shader brdfShader;
    brdfShader.load(enginePath("shaders/brdf.vertex"), enginePath("shaders/brdf.fragment"));

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
        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f,
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Cleanup BRDF state
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &brdfFBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    brdfShader.destroy();

    lineShader.load(enginePath("shaders/debugLine.vertex"), enginePath("shaders/debugLine.fragment"));
    outlineShader.load(enginePath("shaders/outline.vertex"), enginePath("shaders/outline.fragment"));
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(DebugVertex) * 1000, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void *)(sizeof(glm::vec3)));

    glBindVertexArray(0);

    if (overlayShader.load(
            enginePath("shaders/overlay.vertex"),
            enginePath("shaders/overlay.fragment")))
    {
        glGenVertexArrays(1, &overlayVAO);
        glGenBuffers(1, &overlayVBO);
        glBindVertexArray(overlayVAO);
        glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(sizeof(OverlayVertex)),
            reinterpret_cast<const void *>(offsetof(OverlayVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 4, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(sizeof(OverlayVertex)),
            reinterpret_cast<const void *>(offsetof(OverlayVertex, color)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glGenBuffers(1, &sceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, sceneUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, sceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLint64 maxStorageBlockSize = 0;
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxStorageBlockSize);
    if (maxStorageBlockSize > 0)
    {
        maxPointLightCount = static_cast<std::size_t>(maxStorageBlockSize) / sizeof(PointLightGPU);
    }
    glGenBuffers(1, &pointLightsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointLightsSSBO);
    if (maxPointLightCount > 0)
    {
        pointLightBufferCapacity = 1;
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLightGPU), nullptr, GL_DYNAMIC_DRAW);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pointLightsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    Log::Print("Renderer Initialized Successfully", "ENGINE", LogType::LOG_SUCCESS);
}

void Renderer::addPointLight(const PointLight &light)
{
    pointLights.push_back(light);
}

void Renderer::bindDefaultMaterial()
{
    basicShader.setVec4("uBaseColorFactor", glm::vec4(1.0f));
    basicShader.setFloat("uMetallicFactor", 0.0f);
    basicShader.setFloat("uRoughnessFactor", 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, defaultNormalTexture);
}

void Renderer::bindMaterial(const Material &material, const std::vector<Texture> &modelTextures)
{
    basicShader.setVec4("uBaseColorFactor", material.baseColorFactor);
    basicShader.setFloat("uMetallicFactor", material.metallicFactor);
    basicShader.setFloat("uRoughnessFactor", material.roughnessFactor);

    glActiveTexture(GL_TEXTURE0); // base colour
    if (material.baseColorTextureIndex >= 0 && material.baseColorTextureIndex < static_cast<int>(modelTextures.size()))
    {
        glBindTexture(GL_TEXTURE_2D, modelTextures[material.baseColorTextureIndex].getID());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
    }

    glActiveTexture(GL_TEXTURE6); // metallic roughness
    if (material.metallicRoughnessTextureIndex >= 0 &&
        material.metallicRoughnessTextureIndex < static_cast<int>(modelTextures.size()))
    {
        glBindTexture(GL_TEXTURE_2D, modelTextures[material.metallicRoughnessTextureIndex].getID());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, defaultDataTexture);
    }

    glActiveTexture(GL_TEXTURE7); // normal map
    if (material.normalTextureIndex >= 0 && material.normalTextureIndex < static_cast<int>(modelTextures.size()))
    {
        glBindTexture(GL_TEXTURE_2D, modelTextures[material.normalTextureIndex].getID());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, defaultNormalTexture);
    }
}

void Renderer::drawOverlayTriangles(std::span<const OverlayVertex> vertices)
{
    if (vertices.empty() || overlayVAO == 0)
        return;

    const std::size_t maximum = std::min(
        static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()),
        static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) /
            sizeof(OverlayVertex));
    if (vertices.size() % 3 != 0 || vertices.size() > maximum)
    {
        Log::Print("Invalid overlay triangle batch", "Renderer", LogType::LOG_ERROR);
        return;
    }

    glDepthMask(GL_TRUE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    overlayShader.use();
    overlayShader.setMat4("viewProjection", projection * view);
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);

    // Orphan and upload the borrowed frame data; retain only the GPU buffer object.
    const auto bytes = static_cast<GLsizeiptr>(vertices.size_bytes());
    glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::render(unsigned int width, unsigned int height, float deltaTime,
                      const std::vector<Renderable> &renderables, int destX, int destY,
                      std::span<const OverlayVertex> overlayVertices)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    sceneFramebuffer.resize(width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    if (camera != nullptr)
    {
        projection = glm::perspective(glm::radians(camera->getFov()), (float)width / (float)height, config.nearPlane, config.farPlane);
        view = camera->getViewMatrix();
    }

    frustum.extractFromMatrix(projection * view);

    const std::size_t activePointLightCount = std::min(
        pointLights.size(), std::min(maxPointLightCount, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    if (pointLights.size() > activePointLightCount && !pointLightLimitWarningLogged)
    {
        Log::Print("POINT LIGHT COUNT EXCEEDS GPU STORAGE CAPACITY; EXTRA LIGHTS ARE IGNORED", "Renderer",
                   LogType::LOG_WARNING);
        pointLightLimitWarningLogged = true;
    }

    pointLightStorage.resize(activePointLightCount);
    for (std::size_t i = 0; i < activePointLightCount; ++i)
    {
        const PointLight &light = pointLights[i];
        pointLightStorage[i].position = glm::vec4(light.position, 0.0f);
        pointLightStorage[i].colorIntensity = glm::vec4(light.color, light.intensity);
        pointLightStorage[i].attenuation = glm::vec4(light.constant, light.linear, light.quadratic, 0.0f);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointLightsSSBO);
    if (activePointLightCount > pointLightBufferCapacity)
    {
        const std::size_t doubledCapacity = pointLightBufferCapacity == 0 ? 1 : pointLightBufferCapacity * 2;
        pointLightBufferCapacity = std::min(maxPointLightCount, std::max(activePointLightCount, doubledCapacity));
        const GLsizeiptr bufferSize = static_cast<GLsizeiptr>(pointLightBufferCapacity * sizeof(PointLightGPU));
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    }
    if (activePointLightCount > 0)
    {
        const GLsizeiptr dataSize = static_cast<GLsizeiptr>(activePointLightCount * sizeof(PointLightGPU));
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, dataSize, pointLightStorage.data());
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pointLightsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    const std::size_t shadowCasters = pointShadowMap.ensureCapacity(activePointLightCount);
    if (shadowCasters < activePointLightCount && !pointShadowLimitWarningLogged)
    {
        Log::Print("POINT SHADOW ARRAY CAPACITY EXCEEDED; REMAINING POINT LIGHTS WILL BE UNSHADOWED", "Renderer",
                   LogType::LOG_WARNING);
        pointShadowLimitWarningLogged = true;
    }

    // 1. shadow pass
    shadowMap.beginPass();
    for (const auto &renderable : renderables)
    {
        shadowMap.getDepthShader().setMat4("model", renderable.modelMatrix);
        renderable.model->draw();
    }
    shadowMap.endPass();
    for (std::size_t i = 0; i < shadowCasters; i++)
    {
        glm::vec3 lightPos = pointLights[i].position;

        glm::mat4 captureViews[] = {
            glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

        pointShadowMap.beginPass(i, lightPos, config.nearPlane, config.farPlane);

        for (int face = 0; face < 6; face++)
        {
            pointShadowMap.bindFace(face, captureViews[face]);
            for (const auto &renderable : renderables)
            {
                pointShadowMap.getDepthShader().setMat4("model", renderable.modelMatrix);
                renderable.model->draw();
            }
        }
        pointShadowMap.endPass();
    }

    // bind scene framebuffer for main pass
    sceneFramebuffer.bind();

    // main pass
    glClearColor(config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    basicShader.use();

    SceneUniforms uboData{};
    uboData.view = view;
    uboData.projection = projection;
    uboData.viewPos = camera->getPosition();

    uboData.lightDir = sunLight.direction;
    uboData.lightColor = sunLight.color;
    uboData.lightIntensity = sunLight.intensity;

    uboData.fogColor = config.fogColor;
    uboData.fogDensity = config.fogDensity;
    uboData.fogHeightFalloff = config.fogHeightFalloff;

    uboData.pointLightCount = static_cast<int>(activePointLightCount);
    uboData.pointShadowCount = static_cast<int>(shadowCasters);
    uboData.pointShadowfarPlane = config.farPlane;
    // upload to gpu in one call
    glBindBuffer(GL_UNIFORM_BUFFER, sceneUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneUniforms), &uboData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    basicShader.setMat4("lightSpaceMatrix", shadowMap.getLightSpaceMatrix());

    // Bind textures: 0=diffuse, 1=directional shadow, 2=point-shadow cube array.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap.getDepthMap());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowMap.getDepthCubeMapArray());

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

    for (const auto &renderable : renderables)
    {
        Scene::AABB worldBounds = TransformAABB({renderable.boundsMin, renderable.boundsMax}, renderable.modelMatrix);
        if (!frustum.testAABB(worldBounds.min, worldBounds.max))
            continue;

        basicShader.setMat4("model", renderable.modelMatrix);
        basicShader.setMat4("uNormalMatrix", renderable.normalMatrix);

        for (size_t meshIdx = 0; meshIdx < renderable.model->meshes.size(); meshIdx++)
        {
            auto &modelMesh = renderable.model->meshes[meshIdx];

            if (modelMesh.materialIndex >= 0 &&
                modelMesh.materialIndex < static_cast<int>(renderable.model->materials.size()))
            {
                bindMaterial(renderable.model->materials[modelMesh.materialIndex], renderable.model->textures);
            }
            else
            {
                bindDefaultMaterial();
            }
            modelMesh.draw();
        }
    }

    // grid
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gridShader.use();
    glm::mat4 gridModel = glm::mat4(1.0f);
    gridShader.setMat4("model", gridModel);
    grid.draw();
    glDisable(GL_BLEND);

    // skybox
    skybox.draw(view, projection);

    for (size_t i = 0; i < persistentLines.size();)
    {
        persistentLines[i].lifetime -= deltaTime;

        if (persistentLines[i].lifetime <= 0.0f)
        {
            persistentLines[i] = persistentLines.back();
            persistentLines.pop_back();
        }
        else
        {
            // Line is still alive. Push it to the immediate draw buffer for this
            // frame.
            debugLines.push_back({persistentLines[i].start, persistentLines[i].color});
            debugLines.push_back({persistentLines[i].end, persistentLines[i].color});
            i++;
        }
    }

    if (!debugLines.empty())
    {
        glDisable(GL_DEPTH_TEST);
        lineShader.use();
        lineShader.setMat4("projection", projection);
        lineShader.setMat4("view", view);

        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, debugLines.size() * sizeof(DebugVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, debugLines.size() * sizeof(DebugVertex), debugLines.data());

        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, debugLines.size());
        glLineWidth(1.0f);

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        debugLines.clear();
    }

    if (!outlineTargets.empty() && outlineShader.getId() != 0)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xFF);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);

        outlineShader.use();
        outlineShader.setVec3("outlineColor", glm::vec3(1.0f, 0.45f, 0.0f));

        const auto drawSelected = [&]()
        {
            for (const auto &renderable : renderables)
            {
                if (std::find(outlineTargets.begin(), outlineTargets.end(),
                              renderable.entityId) == outlineTargets.end())
                    continue;

                outlineShader.setMat4("mvp", projection * view * renderable.modelMatrix);

                for (auto &mesh : renderable.model->meshes)
                    mesh.draw();
            }
        };
        
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        glDisable(GL_DEPTH_TEST);
        outlineShader.setVec3("clipOffset", glm::vec3(0.0f));
        drawSelected();
        glEnable(GL_DEPTH_TEST);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        constexpr glm::vec2 offsets[] = {
            {-1.0f, -1.0f}, {0.0f, -1.0f}, {1.0f, -1.0f},
            {-1.0f,  0.0f},                 {1.0f,  0.0f},
            {-1.0f,  1.0f}, {0.0f,  1.0f}, {1.0f,  1.0f}};

        constexpr float thicknessPixels = 2.0f;
        const glm::vec2 pixelOffset{2.0f * thicknessPixels / static_cast<float>(width), 2.0f * thicknessPixels / static_cast<float>(height)};

        for (const auto &offset : offsets)
        {
            outlineShader.setVec3("clipOffset", glm::vec3(offset * pixelOffset, 0.0f));
            drawSelected();
        }

        glStencilMask(0xFF);
        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    drawOverlayTriangles(overlayVertices);
    sceneFramebuffer.unbind();

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    sceneFramebuffer.blitToScreen(destX, destY, width, height);
}

void Renderer::shutdown()
{
    grid.destroy();
    gridShader.destroy();
    lineShader.destroy();
    outlineShader.destroy();
    overlayShader.destroy();
    glDeleteVertexArrays(1, &overlayVAO);
    glDeleteBuffers(1, &overlayVBO);
    overlayVAO = 0;
    overlayVBO = 0;
    basicShader.destroy();
    skybox.destroy();
    sceneFramebuffer.destroy();
    shadowMap.destroy();
    pointShadowMap.destroy();

    if (lineVAO != 0)
    {
        glDeleteVertexArrays(1, &lineVAO);
        lineVAO = 0;
    }
    if (lineVBO != 0)
    {
        glDeleteBuffers(1, &lineVBO);
        lineVBO = 0;
    }
    if (sceneUBO != 0)
    {
        glDeleteBuffers(1, &sceneUBO);
        sceneUBO = 0;
    }
    if (defaultDataTexture != 0)
    {
        glDeleteTextures(1, &defaultDataTexture);
        defaultDataTexture = 0;
    }
    if (defaultNormalTexture != 0)
    {
        glDeleteTextures(1, &defaultNormalTexture);
        defaultNormalTexture = 0;
    }
    if (brdfLUTTexture != 0)
    {
        glDeleteTextures(1, &brdfLUTTexture);
        brdfLUTTexture = 0;
    }

    if (pointLightsSSBO != 0)
    {
        glDeleteBuffers(1, &pointLightsSSBO);
        pointLightsSSBO = 0;
    }

    Log::Print("Renderer shutdown successfully", "ENGINE", LogType::LOG_SUCCESS);
}

Scene::AABB Renderer::TransformAABB(const Scene::AABB &localBounds, const glm::mat4 &modelMatrix)
{
    // get all 8 corners of the local AABB
    glm::vec3 corners[8] = {glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.min.z),
                            glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.min.z),
                            glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.min.z),
                            glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.min.z),
                            glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.max.z),
                            glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.max.z),
                            glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.max.z),
                            glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.max.z)};

    // transform each corner to world space and compute new min/max
    glm::vec3 worldMin = glm::vec3(FLT_MAX);
    glm::vec3 worldMax = glm::vec3(-FLT_MAX);

    for (int i = 0; i < 8; i++)
    {
        glm::vec4 worldCorner = modelMatrix * glm::vec4(corners[i], 1.0f);
        worldMin = glm::min(worldMin, glm::vec3(worldCorner));
        worldMax = glm::max(worldMax, glm::vec3(worldCorner));
    }

    Scene::AABB worldBounds{worldMin, worldMax};
    return worldBounds;
}

void Renderer::addDebugLine(const glm::vec3 &start, const glm::vec3 &end, const glm::vec3 &color, float duration)
{
    if (duration <= 0.0f)
    {
        // (disappears next frame)
        debugLines.push_back({start, color});
        debugLines.push_back({end, color});
    }
    else
    {
        // (stays on screen)
        persistentLines.push_back({start, end, color, duration});
    }
}

void Renderer::setDirectionalLight(const DirectionalLight &light)
{
    sunLight = light;
    shadowMap.setLightDir(light.direction);
}

void Renderer::setPointLights(const std::vector<PointLight> &lights)
{
    pointLights = lights;
}

} // namespace Cthulhu::Rendering
