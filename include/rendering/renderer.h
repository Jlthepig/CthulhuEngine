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

namespace Cthulhu::Rendering
{
    class Renderer
    {
        public:
        void setScene(Cthulhu::Scene::Scene* scene);
        void init(GLFWwindow* window, Scene::Camera* camera);
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
        PointLight pointLight;
        ShadowMap shadowMap;
        Cthulhu::Scene::Scene* scene = nullptr;
        Frustum frustum;
        Scene::AABB TransformAABB(const Scene::AABB& localBounds, const glm::mat4& modelMatrix);
    };
}