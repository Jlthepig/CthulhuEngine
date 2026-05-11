#pragma once

#include "glm.hpp"
#include "shader.h"

namespace Cthulhu::Rendering
{
    class PointLightShadowMap
    {
    public:
        void init(unsigned int width, unsigned int height);
        void beginPass(glm::vec3 lightPos, float nearPlane, float farPlane);
        void bindFace(int face, const glm::mat4& viewMatrix);
        void endPass();

        unsigned int getDepthCubeMap() const { return depthCubeMap; }
        Shader& getDepthShader() { return depthShader; }

    private:
        unsigned int depthMapFBO = 0;
        unsigned int depthCubeMap = 0;
        unsigned int rbo = 0;
        unsigned int shadowWidth = 1024, shadowHeight = 1024;
        Shader depthShader;
    };
}