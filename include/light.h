#pragma once

#include "glm.hpp"

namespace Cthulhu::Rendering
{
    struct DirectionalLight
    {
        glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 2.0f;
    };
}