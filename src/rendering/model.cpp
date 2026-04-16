#include "model.h"

namespace Cthulhu::Rendering
{
    void Model::draw()
    {
        size_t index = 0;
        for (auto& Mesh : meshes)
        {
            if (index< textures.size())
            {
                textures[index].bind(0);
            }
            Mesh.draw();
            index++;
        }
    }
    
    void Model::destroy()
    {
        for (auto& Mesh : meshes) {  Mesh.destroy();}
        for (auto& Tex: textures) {Tex.destroy();}
        
         
        
    }
}