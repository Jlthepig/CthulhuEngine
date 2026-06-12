#include "pch.h"
#include "pointShadowMap.h"
#include "log_utils.hpp"
#include "ext/matrix_clip_space.hpp"

namespace Cthulhu::Rendering
{
    void PointLightShadowMap::init(unsigned int width, unsigned int height)
    {
        shadowWidth = width;
        shadowHeight = height;

        // Cubemap stores distance from light (not depth)
        glGenTextures(1, &depthCubeMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeMap);
        for (unsigned int i = 0; i < 6; i++)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R32F,
                shadowWidth, shadowHeight, 0, GL_RED, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Depth renderbuffer just for depth testing during shadow pass
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, shadowWidth, shadowHeight);

        // FBO
        glGenFramebuffers(1, &depthMapFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X, depthCubeMap, 0);

        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_NONE);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            KalaHeaders::KalaLog::Log::Print("POINT SHADOW FBO NOT COMPLETE: " + std::to_string(status), "PointShadowMap", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        }
        else
        {
            KalaHeaders::KalaLog::Log::Print("Point shadow FBO complete", "PointShadowMap", KalaHeaders::KalaLog::LogType::LOG_SUCCESS);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        depthShader.load("shaders/point_depth.vertex", "shaders/point_depth.fragment");
    }

    void PointLightShadowMap::beginPass(glm::vec3 lightPos, float nearPlane, float farPlane)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glViewport(0, 0, shadowWidth, shadowHeight);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // cleared = max distance = no shadow

        depthShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
        depthShader.setMat4("projection", projection);
        depthShader.setVec3("lightPos", lightPos);
        depthShader.setFloat("farPlane", farPlane);
    }

    void PointLightShadowMap::bindFace(int face, const glm::mat4& viewMatrix)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depthCubeMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        depthShader.setMat4("view", viewMatrix);
    }

    void PointLightShadowMap::endPass()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}