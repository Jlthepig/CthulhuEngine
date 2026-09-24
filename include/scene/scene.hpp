#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <string_view>

#include <flecs.h>

#include "EntityId.hpp"
#include "components.hpp"
#include "light.hpp"
#include "model.hpp"

namespace Cthulhu::Scene
{
class Scene
{
  public:
    const std::optional<std::string>& getResourcePath() const
    {
        return resourcePath;
    }

    bool hasResourcePath() const
    {
        return resourcePath.has_value();
    }
    bool isDirty() const
    {
        return dirty;
    }
    bool needsSave() const
    {
        return dirty || !resourcePath.has_value();
    }

    void markDirty()
    {
        dirty = true;
    }
    void markClean()
    {
        dirty = false;
    }

    void setResourcePath(std::string path)
    {
        resourcePath = std::move(path);
    }
    void clearResourcePath()
    {
        resourcePath.reset();
    }

    flecs::world &getWorld()
    {
        return world;
    }
    const flecs::world &getWorld() const
    {
        return world;
    }

    flecs::entity createEntity(const std::string &name = "Entity");

    [[nodiscard]]
    std::optional<flecs::entity> findEntity(EntityId id) const;

    [[nodiscard]]
    bool isEntityAlive(EntityId id) const;    
    bool renameEntity(EntityId id, std::string_view newName);
    bool destroyEntity(EntityId id);


    Rendering::Model *getOrLoadModel(const std::string &resourcePath, const std::filesystem::path &fileSystemPath);
    void clear();

    void setDirectionalLight(const Rendering::DirectionalLight &light)
    {
        directionalLight = light;
        markDirty();
    }
    void addPointLight(const Rendering::PointLight &light)
    {
        pointLights.push_back(light);
        markDirty();
    }

    const Rendering::DirectionalLight &getDirectionalLight() const
    {
        return directionalLight;
    }
    const std::vector<Rendering::PointLight> getPointLights() const
    {
        return pointLights;
    }

    void setName(const std::string &n)
    {
        if (name == n) 
        {
            return;
        }

        name = n;
        markDirty();
    }
    const std::string &getName() const
    {
        return name;
    }

  private:
    std::string name;
    std::optional<std::string>  resourcePath;
    bool dirty = false;
    uint32_t nextId = 0;

    flecs::world world;
    std::unordered_map<EntityId, flecs::entity, EntityIdHash> entityLookup;

    Rendering::DirectionalLight directionalLight;
    std::vector<Rendering::PointLight> pointLights;
    std::unordered_map<std::string, Rendering::Model> modelCache;

    bool registerEntity(EntityId id, flecs::entity entity);
    void unregisterEntity(EntityId id);

    [[nodiscard]]
    EntityId generateUniqueEntityId() const;
};
} // namespace Cthulhu::Scene