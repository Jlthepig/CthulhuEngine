#pragma once

#include "transform.h"
#include "model.h"
#include <string>
namespace Cthulhu::Scene 
{
    struct AABB
    {
        glm::vec3 min = glm::vec3(-0.5f);
        glm::vec3 max = glm::vec3(0.5f);
    };
    struct Entity
    {
        std::string name;
        bool active = true;
        uint32_t id = 0;
        

        Transform transform;
        Rendering::Model* model = nullptr;
        AABB bounds;

    };
}