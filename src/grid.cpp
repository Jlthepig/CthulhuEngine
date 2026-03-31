#include "fwd.hpp"
#include "glad.h"
#include "glfw3.h"
#include "glm.hpp"
#include <algorithm>
#include <vector>
#include "grid.h"

namespace Cthulhu::Rendering
{
    void GridLines::setupGrid(int size)
    {
        std::vector<float> lines;
        for (int i = -size;i <=size;i++)
        {
            lines.push_back(-size);
            lines.push_back(0.0f);
            lines.push_back(i);

            lines.push_back(size);
            lines.push_back(0.0f);
            lines.push_back(i);
        }
        for (int i = -size;i <=size;i++)
        {
            lines.push_back(i);
            lines.push_back(0.0f);
            lines.push_back(-size);

            lines.push_back(i);
            lines.push_back(0.0f);
            lines.push_back(size);
        }

        glGenVertexArrays(1,&VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER,VBO);
        glBufferData(GL_ARRAY_BUFFER,lines.size() * sizeof(float),lines.data(),GL_STATIC_DRAW);

        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0); 
        vertexCount = lines.size() / 3;
    }
    
    void GridLines::draw()
    {
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0,vertexCount);
        glBindVertexArray(0); 
    }
    
    void GridLines::destroy()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    
}