#pragma once

#include "material.h"
#include "mesh.h"
#include "texture.h"
#include <vector>
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