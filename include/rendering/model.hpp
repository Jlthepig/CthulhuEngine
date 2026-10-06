#pragma once

#include <vector>

#include "material.hpp"
#include "mesh.hpp"
#include "texture.hpp"
namespace Cthulhu::Rendering
{
struct Model
{
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};

    void draw();
    void destroy();
};
} // namespace Cthulhu::Rendering