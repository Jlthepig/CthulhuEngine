#pragma once

#include "frustum.h"
#include "shader.h"
#include "grid.h"
#include "light.h"
#include "camera.h"
#include "glad.h"
#include "glfw3.h"
#include "skybox.h"
#include "shadowMap.h"
#include "pointShadowMap.h"
#include <vector>
#include <string>

namespace Cthulhu::Scene { class Scene; }      
namespace Cthulhu::Rendering { struct Model; }
namespace Cthulhu::Rendering
{
    struct PersistentLine {
        glm::vec3 start;
        glm::vec3 end;
        glm::vec3 color;
        float lifetime;
    };
    struct Renderable
    {
        Model* model = nullptr;
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        glm::mat4 normalMatrix = glm::mat4(1.0f);
        glm::vec3 boundsMin = glm::vec3(-1.0f);
        glm::vec3 boundsMax = glm::vec3(1.0f);
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

        // Paths
        std::string basicVertPath = "shaders/basic.vertex";
        std::string basicFragPath = "shaders/basic.fragment";
        std::string gridVertPath = "shaders/grid.vertex";
        std::string gridFragPath = "shaders/grid.fragment";
        std::string skyboxHDRPath = "assets/images/Test2.hdr";

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
        void setScene(Cthulhu::Scene::Scene* scene);
        void init(GLFWwindow* window, Scene::Camera* camera, const RenderConfig& config);
        void addPointLight(const PointLight& light);
        void setDirectionalLight(const DirectionalLight& light);
        void setPointLights(const std::vector<PointLight>& lights);
        void render(float fps, float deltaTime, const std::vector<Renderable>& renderables);
        void addDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color,float duration = 0.0f);
        void shutdown();

        private:
        RenderConfig config; // Store config here
        Cthulhu::Rendering::Shader basicShader;
        Cthulhu::Rendering::Shader gridShader;
        Cthulhu::Rendering::Shader lineShader;
        Cthulhu::Rendering::GridLines grid;
        Cthulhu::Rendering::Skybox skybox;
        glm::mat4 projection;
        glm::mat4 view;
        Scene::Camera* camera = nullptr;
        GLFWwindow* window = nullptr;
        DirectionalLight sunLight;
        static constexpr int MAX_POINT_SHADOW_CASTERS = 4;
        PointLightShadowMap pointShadowMaps[MAX_POINT_SHADOW_CASTERS];
        std::vector<PointLight> pointLights;
        std::vector<DebugVertex> debugLines;
        std::vector<PersistentLine> persistentLines;
        ShadowMap shadowMap;
        Cthulhu::Scene::Scene* scene = nullptr;
        Scene::Frustum frustum;
        Scene::AABB TransformAABB(const Scene::AABB& localBounds, const glm::mat4& modelMatrix);
        GLuint whitePointShadow = 0;
        GLuint defaultDataTexture = 0;
        GLuint defaultNormalTexture = 0;
        GLuint brdfLUTTexture = 0;
        unsigned int lineVAO = 0, lineVBO = 0;
        size_t totalTriangles = 0;
    };
}