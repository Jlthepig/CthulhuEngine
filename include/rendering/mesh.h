#pragma once

#include <cstddef>
#include <vector>
#include "glad.h"
#include "glfw3.h"

namespace Cthulhu::Rendering 
{

    class Mesh
    {
        public:

        struct vertexAttribute
        {
            unsigned int location;
            int componentCount;
            size_t offset;
        };

        int materialIndex = -1;

        void setup(const std::vector<float>& vertices, 
        const std::vector<unsigned int>& indices,
        const std::vector<vertexAttribute>& attributes,
        unsigned int stride   
    );
        size_t getIndexCount() const;
        size_t getVertexDataSize() const;
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