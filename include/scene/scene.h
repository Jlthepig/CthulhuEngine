#pragma once

#include "model.h"
#include "components.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include "flecs.h"

namespace Cthulhu::Scene
{
    class Scene
    {
    public:
        flecs::world& getWorld() { return world; }
        flecs::entity createEntity(const std::string& name = "Entity");

        Rendering::Model* getOrLoadModel(const std::string& modelPath);
        void clear();
    private:
        flecs::world world;
        uint32_t nextId = 0;
        std::unordered_map<std::string, Rendering::Model> modelCache;
    };
}