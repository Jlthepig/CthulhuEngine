#pragma once

#include "frustum.h"
#include "scene.h"
#include "shader.h"
#include "grid.h"
#include "light.h"
#include "camera.h"
#include "glad.h"
#include "glfw3.h"
#include "skybox.h"
#include "shadowMap.h"
#include <vector>

namespace Cthulhu::Rendering
{
    class Renderer
    {
        public:
        void setScene(Cthulhu::Scene::Scene* scene);
        void init(GLFWwindow* window, Scene::Camera* camera);
        void addPointLight(const PointLight& light);
        void setDirectionalLight(const DirectionalLight& light);
        void setPointLights(const std::vector<PointLight>& lights);
        void render(float deltaTime);
        void shutdown();
        private:
        Cthulhu::Rendering::Shader basicShader;
        Cthulhu::Rendering::Shader gridShader;
        Cthulhu::Rendering::GridLines grid;
        Cthulhu::Rendering::Skybox skybox;
        glm::mat4 projection;
        glm::mat4 view;
        Scene::Camera* camera = nullptr;
        GLFWwindow* window = nullptr;
        DirectionalLight sunLight;
        std::vector<PointLight> pointLights;
        ShadowMap shadowMap;
        Cthulhu::Scene::Scene* scene = nullptr;
        Frustum frustum;
        Scene::AABB TransformAABB(const Scene::AABB& localBounds, const glm::mat4& modelMatrix);
        size_t totalTriangles = 0;
    };
    
}