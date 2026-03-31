#pragma once

namespace Cthulhu::Rendering 
{
    class GridLines
    {
        public:
        void setupGrid(int size);
        void draw();
        void destroy();

        private:
        unsigned int VAO;
        unsigned int VBO;
        int vertexCount = 0;
    };

}