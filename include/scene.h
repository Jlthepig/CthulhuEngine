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
        const std::vector<Entity>& getEntities() const;
    private:
        std::vector<Entity> entities;
        uint32_t nextId = 0;
    };
}