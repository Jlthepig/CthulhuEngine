#pragma once

#include "model.h"
#include "light.h"
#include "components.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
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

        void setDirectionalLight(const Rendering::DirectionalLight& light) {directionalLight = light;}
        void addPointLight(const Rendering::PointLight& light) {pointLights.push_back(light);}

        const Rendering::DirectionalLight& getDirectionalLight() const {return directionalLight;}
        const std::vector<Rendering::PointLight> getPointLights() const {return pointLights;}

    private:
        flecs::world world;
        uint32_t nextId = 0;
        std::unordered_map<std::string, Rendering::Model> modelCache;

        Rendering::DirectionalLight directionalLight;
        std::vector<Rendering::PointLight> pointLights;
    };
}