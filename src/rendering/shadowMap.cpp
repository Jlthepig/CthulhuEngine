#include "shadowMap.h"
#include "log_utils.hpp"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"

namespace Cthulhu::Rendering
{
    void ShadowMap::init(unsigned int width, unsigned int height)
    {
        shadowWidth = width;
        shadowHeight = height;

        // create depth texture
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,width, height, 0, GL_DEPTH_COMPONENT, 
            GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // attach to framebuffer
        glGenFramebuffers(1, &depthMapFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // load depth shader
        depthShader.load("shaders/depth.vertex", "shaders/depth.fragment");
    }

    void ShadowMap::setLightDir(const glm::vec3& direction)
    {
        lightDir = direction;
    }

    void ShadowMap::beginPass()
    {
        // build light space matrix
        glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);
        glm::mat4 lightView = glm::lookAt(-lightDir * 20.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        lightSpaceMatrix = lightProjection * lightView;

        depthShader.use();
        depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, shadowWidth, shadowHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);

        glClear(GL_DEPTH_BUFFER_BIT);

        // fix peter panning
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
    }

    void ShadowMap::endPass()
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    unsigned int ShadowMap::getDepthMap() const
    {
        return depthMap;
    }

    glm::mat4 ShadowMap::getLightSpaceMatrix() const
    {
        return lightSpaceMatrix;
    }

    Shader& ShadowMap::getDepthShader()
    {
        return depthShader;
    }

    void ShadowMap::destroy()
    {
        glDeleteFramebuffers(1, &depthMapFBO);
        glDeleteTextures(1, &depthMap);
        depthShader.destroy();
    }
}
