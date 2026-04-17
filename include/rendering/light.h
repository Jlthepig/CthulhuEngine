#pragma once

#include "glm.hpp"

namespace Cthulhu::Rendering
{
    struct DirectionalLight
    {
        glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 0.0f;
    };

    struct PointLight
    {
        glm::vec3 position = glm::vec3(3.0f, 2.0f, 3.0f);
        glm::vec3 color = glm::vec3(1.0f, 0.5f, 0.5f);
        float intensity = 4.0f;
        float radius = 1.0f;

        // Attenuation factors
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    };
}