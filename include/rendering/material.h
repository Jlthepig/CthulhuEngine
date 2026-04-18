#pragma once

#include "fastgltf/types.hpp"
#include <glm/glm.hpp>
namespace Cthulhu::Rendering
{
    struct Material
    {
        int baseColorTextureIndex = -1;
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
    };
}