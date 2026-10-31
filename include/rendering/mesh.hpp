#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <glad.h>
#include <glm.hpp>

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

    void setup(const std::vector<float> &vertices,
               const std::vector<unsigned int> &indices,
               const std::vector<vertexAttribute> &attributes,
               unsigned int stride);
    size_t getIndexCount() const;
    size_t getVertexDataSize() const;

    [[nodiscard]] std::span<const unsigned int> getIndices() const noexcept;
    [[nodiscard]] bool getPosition(size_t index, glm::vec3 &position) const noexcept;

    void draw();
    void destroy();

  private:
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;

    std::vector<float> vertexData;
    std::vector<unsigned int> indexData;

    size_t positionStrideFloats = 0;
    size_t positionOffsetFloats = 0;
};

} // namespace Cthulhu::Rendering
