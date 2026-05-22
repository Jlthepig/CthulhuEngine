#pragma once

#include "fastgltf/types.hpp"
#include <glm/glm.hpp>
namespace Cthulhu::Rendering
{
    struct Material
    {
        int baseColorTextureIndex = -1;
        glm::vec4 baseColorFactor = glm::vec4(1.0f);

        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        int metallicRoughnessTextureIndex = -1;
        int normalTextureIndex = -1;

    };
}