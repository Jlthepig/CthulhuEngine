#pragma once

#include <vector>
#include "glad.h"
#include "glfw3.h"

namespace Cthulhu::Rendering 
{

    class Mesh
    {
        public:
        void setup(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
        void draw();
        void destroy();

        private:
        unsigned int VAO;
        unsigned int VBO;
        unsigned int EBO;

        std::vector<float> vertexData;
        std::vector<unsigned int> indexData;
    };

}