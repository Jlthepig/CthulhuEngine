#pragma once

#include "glm.hpp"
#include <cstdint>


namespace Cthulhu::Rendering { struct Model; }

namespace Cthulhu::Scene
{
    struct TransformComponent
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};

        bool matrixDirty = true;
        glm::mat4 cachedModelMatrix = glm::mat4(1.0f);
        glm::mat4 cachedNormalMatrix = glm::mat4(1.0f);
    };

    struct MeshComponent
    {
        Cthulhu::Rendering::Model* model = nullptr;
        glm::vec3 boundsMin = glm::vec3(-1.0f);
        glm::vec3 boundsMax = glm::vec3(1.0f);
    };

    struct PhysicsComponent
    {
        uint32_t bodyId = 0;
        bool hasBody = false;
    };

    struct TagActive {};
    struct TagStatic {};
}