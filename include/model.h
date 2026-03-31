#pragma once


#include <vector>
#include "mesh.h"
#include "texture.h"
namespace Cthulhu::Rendering
{
    struct Model
    {
        std::vector<Mesh> meshes;
        std::vector<Texture> textures;
        void draw();
        void destroy();
    };
}