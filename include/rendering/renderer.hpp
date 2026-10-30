#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

#include <glad.h>
#include <GLFW/glfw3.h>

#include "camera.hpp"
#include "entityId.hpp"
#include "framebuffer.hpp"
#include "frustum.hpp"

#include "grid.hpp"
#include "light.hpp"
#include "material.hpp"
#include "pointShadowMap.hpp"
#include "sceneUniforms.hpp"
#include "shader.hpp"
#include "shadowMap.hpp"
#include "skybox.hpp"
#include "texture.hpp"

namespace Cthulhu::Scene
{
class Scene;
}
namespace Cthulhu::Rendering
{
struct Model;
}
namespace Cthulhu::Rendering
{
struct PersistentLine
{
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 color;
    float lifetime;
};
struct Renderable
{
    Model *model = nullptr;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::mat4 normalMatrix = glm::mat4(1.0f);
    glm::vec3 boundsMin = glm::vec3(-1.0f);
    glm::vec3 boundsMax = glm::vec3(1.0f);
    Scene::EntityId entityId{};
};

struct DebugVertex
{
    glm::vec3 pos;
    glm::vec3 color;
};
struct RenderConfig
{
    // Camera
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // Shadows
    int shadowMapResolution = 1024;
    float dirLightOrthoSize = 20.0f; // The -20 to 20 range in shadow map

    // World
    float gridSize = 256.0f;
    glm::vec4 clearColor = glm::vec4(0.2f, 0.3f, 0.3f, 1.0f);

    // engine resources root
    std::filesystem::path engineResourceRoot;

    // IBL
    int irradianceMapSize = 32;
    int brdfLUTSize = 512;

    glm::vec3 fogColor = glm::vec3(0.15f, 0.18f, 0.22f); // Moody blue/grey
    float fogDensity = 0.02f;
    float fogHeightFalloff = 0.1f;
};

class Renderer
{
  public:
    void setScene(Cthulhu::Scene::Scene *scene);
    void init(GLFWwindow *window, Scene::Camera *camera, const RenderConfig &config);
    void addPointLight(const PointLight &light);
    void setDirectionalLight(const DirectionalLight &light);
    void setPointLights(const std::vector<PointLight> &lights);
    void setOutlineTargets(std::span<const Scene::EntityId> entities);
    void render(unsigned int width, unsigned int height, float deltaTime, const std::vector<Renderable> &renderables, int destX = 0, int destY = 0);
    void addDebugLine(const glm::vec3 &start, const glm::vec3 &end, const glm::vec3 &color, float duration = 0.0f);
    void shutdown();

    [[nodiscard]] const RenderConfig &getConfig() const noexcept
    {
        return config;
    }

  private:
    RenderConfig config;
    Cthulhu::Rendering::Shader basicShader;
    Cthulhu::Rendering::Shader gridShader;
    Cthulhu::Rendering::Shader lineShader;
    std::vector<Scene::EntityId> outlineTargets;
    Shader outlineShader;
    Cthulhu::Rendering::GridLines grid;
    Cthulhu::Rendering::Skybox skybox;
    Cthulhu::Rendering::Framebuffer sceneFramebuffer;
    glm::mat4 projection;
    glm::mat4 view;
    Scene::Camera *camera = nullptr;
    GLFWwindow *window = nullptr;
    DirectionalLight sunLight;
    PointLightShadowMap pointShadowMap;
    std::vector<PointLight> pointLights;
    std::vector<PointLightGPU> pointLightStorage;
    std::size_t pointLightBufferCapacity = 0;
    std::size_t maxPointLightCount = 0;
    bool pointLightLimitWarningLogged = false;
    bool pointShadowLimitWarningLogged = false;
    std::vector<DebugVertex> debugLines;
    std::vector<PersistentLine> persistentLines;
    ShadowMap shadowMap;
    Cthulhu::Scene::Scene *scene = nullptr;
    Scene::Frustum frustum;
    Scene::AABB TransformAABB(const Scene::AABB &localBounds, const glm::mat4 &modelMatrix);
    GLuint defaultDataTexture = 0;
    GLuint defaultNormalTexture = 0;
    GLuint brdfLUTTexture = 0;
    unsigned int lineVAO = 0, lineVBO = 0;
    unsigned int sceneUBO = 0;
    unsigned int pointLightsSSBO = 0;

    void bindMaterial(const Material &material, const std::vector<Texture> &modelTextures);
    void bindDefaultMaterial();
};
} // namespace Cthulhu::Rendering
