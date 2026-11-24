#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

#include <GLFW/glfw3.h>
#include <glad.h>

#include "camera.hpp"
#include "entityId.hpp"
#include "framebuffer.hpp"
#include "frustum.hpp"

#include "grid.hpp"
#include "light.hpp"
#include "material.hpp"

#include "overlayVertex.hpp"
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
    glm::vec3 pos{};
    glm::vec3 color{};
};

struct ModelPreview
{
    Model *model = nullptr;
    glm::vec3 position{0.0f};
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
    float gridSpacing = 1.0f;
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
    bool init(GLFWwindow *window, Scene::Camera *camera, const RenderConfig &config);
    void addPointLight(const PointLight &light);
    void setDirectionalLight(const DirectionalLight &light);
    void setPointLights(const std::vector<PointLight> &lights);
    void setOutlineTargets(std::span<const Scene::EntityId> entities);
    void render(unsigned int width, unsigned int height,float deltaTime, const std::vector<Renderable> &renderables, int destX = 0, int destY = 0,
                std::span<const OverlayVertex> overlayVertices = {}, const ModelPreview &preview = {});
    void addDebugLine(const glm::vec3 &start, const glm::vec3 &end, const glm::vec3 &color, float duration = 0.0f);
    void shutdown();

    void setCamera(Scene::Camera &value) noexcept
    {
        camera = &value;
    }

    void setGridVisible(bool visible) noexcept
    {
        gridVisible = visible;
    }

    void setScreenClearColor(const glm::vec4 &color) noexcept
    {
        screenClearColor = color;
    }

    [[nodiscard]] const RenderConfig &getConfig() const noexcept
    {
        return config;
    }

  private:
    GLFWwindow *window = nullptr;
    Cthulhu::Scene::Scene *scene = nullptr;
    Scene::Camera *camera = nullptr;

    glm::mat4 projection;
    glm::mat4 view;
    Scene::Frustum frustum;

    RenderConfig config;

    Cthulhu::Rendering::Shader basicShader;
    Cthulhu::Rendering::Shader gridShader;
    Cthulhu::Rendering::Shader lineShader;
    Shader outlineShader;
    Shader overlayShader;

    unsigned int sceneUBO{};
    unsigned int pointLightsSSBO{};

    GLuint overlayVAO{};
    GLuint overlayVBO{};

    unsigned int lineVAO{};
    unsigned int lineVBO{};

    GLuint defaultDataTexture{};
    GLuint defaultNormalTexture{};
    GLuint brdfLUTTexture{};

    Cthulhu::Rendering::Framebuffer sceneFramebuffer;
    std::vector<Scene::EntityId> outlineTargets;

    Cthulhu::Rendering::GridLines grid;
    Cthulhu::Rendering::Skybox skybox;

    DirectionalLight sunLight;
    ShadowMap shadowMap;

    std::vector<PointLight> pointLights;
    std::vector<PointLightGPU> pointLightStorage;
    PointLightShadowMap pointShadowMap;

    std::size_t pointLightBufferCapacity{};
    std::size_t maxPointLightCount{};

    bool pointLightLimitWarningLogged = false;
    bool pointShadowLimitWarningLogged = false;

    bool gridVisible = false;

    glm::vec4 screenClearColor{0.0f, 0.0f, 0.0f, 1.0f};

    std::vector<DebugVertex> debugLines;
    std::vector<PersistentLine> persistentLines;

    static Scene::AABB TransformAABB(const Scene::AABB &localBounds, const glm::mat4 &modelMatrix);

    void bindMaterial(const Material &material, const std::vector<Texture> &modelTextures);
    void bindDefaultMaterial();

    std::size_t uploadPointLights();

    void drawSceneGeometry(const std::vector<Renderable> &renderables, std::size_t activePointLightCount, std::size_t shadowCasters);
    void drawShadows(const std::vector<Renderable> &renderables, std::size_t shadowCasters);
    void drawOutlines(unsigned int width, unsigned int height, const std::vector<Renderable> &renderables);
    void drawOverlayTriangles(std::span<const OverlayVertex> vertices);

    void drawDebugLines(float deltaTime);
};
} // namespace Cthulhu::Rendering
