#include "mesh.hpp"

namespace Cthulhu::Rendering
{
void Mesh::setup(const std::vector<float> &vertices,
                 const std::vector<unsigned int> &indices,
                 const std::vector<vertexAttribute> &attributes,
                 unsigned int stride)
{
    vertexData = vertices;
    indexData = indices;

    positionStrideFloats = 0;
    positionOffsetFloats = 0;

    for (const auto &attr : attributes)
    {
        if (attr.location != 0 || attr.componentCount != 3)
            continue;

        const size_t strideBytes = stride == 0 ? 3 * sizeof(float) : stride;

        if (strideBytes % sizeof(float) != 0 || attr.offset % sizeof(float) != 0 || strideBytes < 3 * sizeof(float) ||
            attr.offset > strideBytes - 3 * sizeof(float))
        {
            break;
        }

        const size_t strideFloats = strideBytes / sizeof(float);
        if (vertexData.size() % strideFloats != 0)
            break;

        positionStrideFloats = strideFloats;
        positionOffsetFloats = attr.offset / sizeof(float);
        break;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    for (const auto &attr : attributes)
    {
        glVertexAttribPointer(attr.location, attr.componentCount, GL_FLOAT, GL_FALSE, stride, (void *)attr.offset);
        glEnableVertexAttribArray(attr.location);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

size_t Mesh::getIndexCount() const
{
    return indexData.size();
}

size_t Mesh::getVertexDataSize() const
{
    return vertexData.size();
}

std::span<const unsigned int> Mesh::getIndices() const noexcept
{
    return {indexData.data(), indexData.size()};
}

bool Mesh::getPosition(size_t index, glm::vec3 &position) const noexcept
{
    if (positionStrideFloats == 0 || index >= vertexData.size() / positionStrideFloats)
    {
        return false;
    }

    const size_t offset = index * positionStrideFloats + positionOffsetFloats;

    position = glm::vec3{vertexData[offset], vertexData[offset + 1], vertexData[offset + 2]};
    return true;
}

void Mesh::draw()
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexData.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Mesh::destroy()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}
} // namespace Cthulhu::Rendering
