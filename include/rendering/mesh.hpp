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
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;
    
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
    unsigned int VAO{};
    unsigned int VBO{};
    unsigned int EBO{};

    std::vector<float> vertexData;
    std::vector<unsigned int> indexData;

    size_t positionStrideFloats{};
    size_t positionOffsetFloats{};
};

} // namespace Cthulhu::Rendering
