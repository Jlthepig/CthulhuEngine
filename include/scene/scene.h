#pragma once

#include "entity.h"
#include <cstdint>
#include <vector>

namespace Cthulhu::Scene
{
    class Scene
    {
    public:
        Entity& addEntity(Entity entity);
        std::vector<Entity>& getEntities();
    private:
        std::vector<Entity> entities;
        uint32_t nextId = 0;
    };
}