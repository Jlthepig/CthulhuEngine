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
        void generateIrradianceMap();
        unsigned int getIrradianceMap() const { return irradianceMap; }
        void generatePrefilterMap();
        unsigned int getPrefilterMap() const { return prefilterMap; }
        void destroy();

        private:
        unsigned int hdrTexture = 0;
        unsigned int cubemapTexture = 0;
        unsigned int captureFBO = 0;
        unsigned int captureRBO = 0;
        unsigned int irradianceMap = 0;
        unsigned int prefilterMap = 0;   
        unsigned int cubeVAO = 0;
        unsigned int cubeVBO = 0;
        Mesh cubeMesh;
        Shader equirectShader;
        Shader skyboxShader;

        void loadHDR(const std::string& path);
        void convertToCubemap();
        void setupCube();
        void renderCube();
        bool isLoaded = false;
    };

}