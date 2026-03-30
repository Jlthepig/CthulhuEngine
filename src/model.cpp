#include "model.h"

namespace Cthulhu::Rendering
{
    void Model::draw()
    {
        for (auto& Mesh : meshes)
        {
            Mesh.draw();
        }
    }
    
    void Model::destroy()
    {
        for (auto& Mesh : meshes)
        {
           Mesh.destroy();
        }
    }
}