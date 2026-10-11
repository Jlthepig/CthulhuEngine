#pragma once

#include <cstddef>
#include <filesystem>

#include "shader.hpp"
namespace Cthulhu::Rendering
{
class PointLightShadowMap
{
  public:
    void init(unsigned int width, unsigned int height, const std::filesystem::path &engineResourceRoot);
    std::size_t ensureCapacity(std::size_t requestedCubeCount);
    void beginPass(std::size_t cubeIndex, glm::vec3 lightPos, float nearPlane, float farPlane);
    void bindFace(int face, const glm::mat4 &viewMatrix);
    void endPass();
    void destroy();

    unsigned int getDepthCubeMapArray() const
    {
        return depthCubeMapArray;
    }
    Shader &getDepthShader()
    {
        return depthShader;
    }

  private:
    unsigned int depthMapFBO = 0;
    unsigned int depthCubeMapArray = 0;
    unsigned int rbo = 0;
    unsigned int shadowWidth = 1024, shadowHeight = 1024;
    std::size_t allocatedCubeCount = 0;
    std::size_t failedCubeCount = 0;
    std::size_t maxCubeCount = 0;
    std::size_t currentCubeIndex = 0;
    Shader depthShader;
};
} // namespace Cthulhu::Rendering