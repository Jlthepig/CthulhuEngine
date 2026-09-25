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
    // << serialization >>
    [[nodiscard]] const std::optional<std::string>& getResourcePath() const noexcept
    {
        return resourcePath;
    }
    [[nodiscard]] bool hasResourcePath() const noexcept
    {
        return resourcePath.has_value();
    }
    [[nodiscard]] bool isDirty() const noexcept
    {
        return dirty;
    }
    [[nodiscard]] bool needsSave() const noexcept
    {
        return dirty || !resourcePath.has_value();
    }

    void markDirty() noexcept
    {
        dirty = true;
    }
    void markClean() noexcept
    {
        dirty = false;
    }

    void setResourcePath(std::string path)
    {
        resourcePath = std::move(path);
    }
    void clearResourcePath() noexcept
    {
        resourcePath.reset();
    }

    // << ecs >>
    [[nodiscard]] flecs::world &getWorld() noexcept
    {
        return world;
    }
    [[nodiscard]] const flecs::world &getWorld() const noexcept
    {
        return world;
    }

    // << entity management >>
    flecs::entity createEntity(const std::string &name = "Entity");
    [[nodiscard]] std::optional<flecs::entity> createEntityWithId(EntityId id, const std::string& name);

    [[nodiscard]] std::optional<flecs::entity> findEntity(EntityId id) const;
    [[nodiscard]] bool isEntityAlive(EntityId id) const;

    bool renameEntity(EntityId id, std::string_view newName);
    bool destroyEntity(EntityId id);
    bool setParent(EntityId childId, EntityId parentId);
    bool clearParent(EntityId childId);

    [[nodiscard]] std::optional<EntityId> getParent(EntityId childId) const;
    [[nodiscard]] std::vector<EntityId> getChildren(EntityId parentId) const;

    [[nodiscard]] std::optional<EntityId> duplicateEntity(EntityId sourceId);

    void clear();

    // << asset lighting >> 
    Rendering::Model *getOrLoadModel(const std::string &resourcePath, const std::filesystem::path &fileSystemPath);

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

    [[nodiscard]] const Rendering::DirectionalLight &getDirectionalLight() const noexcept
    {
        return directionalLight;
    }
    [[nodiscard]] const std::vector<Rendering::PointLight> &getPointLights() const noexcept
    {
        return pointLights;
    }

    // << properties >>
    void setName(const std::string &n)
    {
        if (name == n)
        {
            return;
        }

        name = n;
        markDirty();
    }
    [[nodiscard]] const std::string &getName() const noexcept
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

    [[nodiscard]] bool shouldCreateHierarchyCycle(flecs::entity child, flecs::entity newParent) const;
    
    void collectSubtreeEntityIds(flecs::entity entity, std::vector<EntityId>& ids) const;

    [[nodiscard]] std::optional<EntityId> duplicateEntityRecursive(flecs::entity sourceEntity, std::optional<EntityId> parentId); 
    
    void copyAuthoringComponents(flecs::entity sourceEntity, flecs::entity destinationEntity);

    [[nodiscard]]
    EntityId generateUniqueEntityId() const;
};
} // namespace Cthulhu::Scene