#pragma once


#include <vector>
#include "mesh.h"
namespace Cthulhu::Rendering
{
    struct Model
    {
        std::vector<Mesh> meshes;
        void draw();
        void destroy();
    };
}