#pragma once

#include "glad.h"
#include "glm.hpp"
#include "shader.h"
#include "model.h"

namespace Cthulhu::Rendering
{
    class ShadowMap
    {
        public:
        void init(unsigned int width, unsigned int height);
        void beginPass();
        void endPass();
        void destroy();

        unsigned int getDepthMap() const;
        Shader& getDepthShader();
        glm::mat4 getLightSpaceMatrix() const;
        void setLightDir(const glm::vec3& direction);

        private:
        unsigned int depthMapFBO = 0;
        unsigned int depthMap = 0;
        unsigned int shadowWidth = 1024, shadowHeight = 1024;
        glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
        glm::vec3 lightDir = glm::vec3(-0.2f, -1.0f, -0.3f);

        Shader depthShader;
    };
}