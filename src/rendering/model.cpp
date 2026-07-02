#include "pch.h"
#include "model.h"
namespace Cthulhu::Rendering
{
    void Model::draw()
    {
        for (auto& Mesh : meshes)
        {
            if (Mesh.materialIndex >= 0 && Mesh.materialIndex < std::ssize(materials))
            {
                auto& material = materials[Mesh.materialIndex];
                if (material.baseColorTextureIndex >= 0 && material.baseColorTextureIndex < std::ssize(textures))
                {
                    textures[material.baseColorTextureIndex].bind(0);
                }
            }  

            Mesh.draw();
        }
    }
    
    void Model::destroy()
    {
        for (auto& Mesh : meshes) {  Mesh.destroy();}
        for (auto& Tex: textures) {Tex.destroy();} 
    }
}