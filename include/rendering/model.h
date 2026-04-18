#pragma once


#include <vector>
#include "mesh.h"
#include "texture.h"
#include "material.h"
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
}