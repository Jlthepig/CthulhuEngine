#pragma once

#include "glad.h"
#include "glm.hpp"
#include "shader.h"
#include "mesh.h"
#include <string>
namespace Cthulhu::Rendering
{
    class Skybox
    {

        public:
        void load(const std::string& hdrPath);
        void draw(GLFWwindow* window, const glm::mat4& view, const glm::mat4& projection);
        void destroy();

        private:
        unsigned int hdrTexture = 0;
        unsigned int cubemapTexture = 0;
        unsigned int captureFBO = 0;
        unsigned int captureRBO = 0;

        Mesh cubeMesh;
        Shader equirectShader;
        Shader skyboxShader;

        void loadHDR(const std::string& path);
        void convertToCubemap();
        void setupCube();
        bool isLoaded = false;
    };

}