#pragma once

#include "shader.h"
#include "model.h"
#include "grid.h"
#include "transform.h"
#include "light.h"
#include "camera.h"
#include "glad.h"
#include "glfw3.h"
#include "glm.hpp"
#include "skybox.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace Cthulhu::Rendering
{
    class Renderer
    {
        public:
        void init(GLFWwindow* window, Scene::Camera* camera);
        void render(float deltaTime);
        void shutdown();
        private:
        Cthulhu::Rendering::Shader basicShader;
        Cthulhu::Rendering::Shader gridShader;
        Cthulhu::Rendering::Model fishModel;
        Cthulhu::Rendering::GridLines grid;
        Cthulhu::Scene::Transform fishTransform;
        Cthulhu::Rendering::Skybox skybox;
        glm::mat4 projection;
        glm::mat4 view;
        Scene::Camera* camera = nullptr;
        GLFWwindow* window = nullptr;
        DirectionalLight sunLight;
    };
}