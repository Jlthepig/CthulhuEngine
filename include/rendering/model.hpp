#pragma once

#include <vector>

#include "texture.hpp"
#include "material.hpp"
#include "mesh.hpp"
namespace Cthulhu::Rendering
{
struct Model
{
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    void draw();
    void destroy();
};
} // namespace Cthulhu::Rendering