#pragma once


#include "fwd.hpp"
#include "glm.hpp"

namespace Cthulhu::Scene
{
    struct Transform
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);

        glm::mat4 getModelMatrix() const;
    };
}