#include <glad.h>

#include "grid.hpp"

namespace Cthulhu::Rendering
{

void GridLines::setupGrid()
{
    glGenVertexArrays(1, &VAO);
}

void GridLines::draw()
{
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void GridLines::destroy()
{
    glDeleteVertexArrays(1, &VAO);
    VAO = 0;
}

} // namespace Cthulhu::Rendering
