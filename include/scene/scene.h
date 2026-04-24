#pragma once

#include "entity.h"
#include "model.h"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace Cthulhu::Scene
{
    class Scene
    {
    public:
        Entity& addEntity(Entity entity);
        std::vector<Entity>& getEntities();

        Rendering::Model* getOrLoadModel(const std::string& modelPath);
        void clear();
    private:
        std::vector<Entity> entities;
        uint32_t nextId = 0;
        std::unordered_map<std::string, Rendering::Model> modelCache;
    };
}