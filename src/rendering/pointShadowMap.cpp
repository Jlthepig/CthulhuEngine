#include <algorithm>
#include <ext/matrix_clip_space.hpp>
#include <mat4x4.hpp>
#include <trigonometric.hpp>
#include <vec3.hpp>

#include "log_utils.hpp"
#include "pointShadowMap.hpp"

namespace Cthulhu::Rendering
{
void PointLightShadowMap::init(unsigned int width, unsigned int height, const std::filesystem::path &engineResourceRoot)
{
    shadowWidth = width;
    shadowHeight = height;

    GLint maxArrayLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayLayers);
    maxCubeCount = maxArrayLayers > 0 ? static_cast<std::size_t>(maxArrayLayers) / 6 : 0;

    glGenTextures(1, &depthCubeMapArray);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, depthCubeMapArray);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, shadowWidth, shadowHeight);

    glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depthShader.load((engineResourceRoot / "shaders/point_depth.vertex").string(),
                     (engineResourceRoot / "shaders/point_depth.fragment").string());
}

std::size_t PointLightShadowMap::ensureCapacity(std::size_t requestedCubeCount)
{
    const std::size_t targetCubeCount = std::min(requestedCubeCount, maxCubeCount);
    if (targetCubeCount <= allocatedCubeCount)
    {
        return targetCubeCount;
    }
    if (targetCubeCount <= failedCubeCount)
    {
        return std::min(allocatedCubeCount, targetCubeCount);
    }

    const GLsizei layerCount = static_cast<GLsizei>(targetCubeCount * 6);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, depthCubeMapArray);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY,
                 0,
                 GL_R32F,
                 static_cast<GLsizei>(shadowWidth),
                 static_cast<GLsizei>(shadowHeight),
                 layerCount,
                 0,
                 GL_RED,
                 GL_FLOAT,
                 nullptr);
    GLint allocatedLayerCount = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_TEXTURE_DEPTH, &allocatedLayerCount);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    if (allocatedLayerCount < layerCount)
    {
        KalaHeaders::KalaLog::Log::Print("POINT SHADOW ARRAY ALLOCATION FAILED",
                                         "PointShadowMap",
                                         KalaHeaders::KalaLog::LogType::LOG_ERROR);
        failedCubeCount = targetCubeCount;
        return std::min(allocatedCubeCount, targetCubeCount);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, depthCubeMapArray, 0, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        KalaHeaders::KalaLog::Log::Print("POINT SHADOW ARRAY FBO NOT COMPLETE: " + std::to_string(status),
                                         "PointShadowMap",
                                         KalaHeaders::KalaLog::LogType::LOG_ERROR);
        failedCubeCount = targetCubeCount;
        return std::min(allocatedCubeCount, targetCubeCount);
    }

    allocatedCubeCount = targetCubeCount;
    failedCubeCount = 0;
    return targetCubeCount;
}

void PointLightShadowMap::beginPass(std::size_t cubeIndex, glm::vec3 lightPos, float nearPlane, float farPlane)
{
    currentCubeIndex = cubeIndex;
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, shadowWidth, shadowHeight);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // cleared = max distance = no shadow

    depthShader.use();
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
    depthShader.setMat4("projection", projection);
    depthShader.setVec3("lightPos", lightPos);
    depthShader.setFloat("farPlane", farPlane);
}

void PointLightShadowMap::bindFace(int face, const glm::mat4 &viewMatrix)
{
    if (face < 0 || face >= 6 || currentCubeIndex >= allocatedCubeCount)
    {
        return;
    }

    const GLint layer = static_cast<GLint>(currentCubeIndex * 6 + static_cast<std::size_t>(face));
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, depthCubeMapArray, 0, layer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    depthShader.setMat4("view", viewMatrix);
}

void PointLightShadowMap::endPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PointLightShadowMap::destroy()
{
    depthShader.destroy();
    if (depthMapFBO != 0)
    {
        glDeleteFramebuffers(1, &depthMapFBO);
        depthMapFBO = 0;
    }
    if (depthCubeMapArray != 0)
    {
        glDeleteTextures(1, &depthCubeMapArray);
        depthCubeMapArray = 0;
    }
    if (rbo != 0)
    {
        glDeleteRenderbuffers(1, &rbo);
        rbo = 0;
    }
    allocatedCubeCount = 0;
    failedCubeCount = 0;
    currentCubeIndex = 0;
}
} // namespace Cthulhu::Rendering
