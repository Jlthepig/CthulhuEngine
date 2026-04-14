#pragma once

#include "transform.h"
#include "model.h"
#include <string>
namespace Cthulhu::Scene 
{
    struct Entity
    {
        std::string name;
        bool active = true;
        uint32_t id = 0;
        

        Transform transform;
        Rendering::Model* model = nullptr;

    };

}