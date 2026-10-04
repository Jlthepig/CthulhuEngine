#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <string_view>

#include <flecs.h>

#include "entityId.hpp"
#include "componentRegistry.hpp"
#include "light.hpp"

namespace Cthulhu::Scene
{
struct AssetReference
{
    EntityId entity;
    std::string component;
    std::string field;
};

struct EntitySnapshot
{
    EntityId id;
    std::optional<EntityId> parentId;
    std::string name;
    std::vector<ComponentSnapshot> components;
};
class Scene
{
  public:
    explicit Scene(const ComponentRegistry& registry) : componentRegistry(&registry)
    {
    }

    [[nodiscard]] const ComponentRegistry* getComponentRegistry() const noexcept
    {
        return componentRegistry;
    }

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

    [[nodiscard]] std::optional<std::vector<EntitySnapshot>> captureSubtree(EntityId rootId) const;

    bool restoreSubtree(const std::vector<EntitySnapshot>& snapshots);
    // every asset field in this scene that points at resourcePath
    [[nodiscard]] std::vector<AssetReference> findAssetReferences(std::string_view resourcePath) const;

    void clear();

    // << asset lighting >> 
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
    const ComponentRegistry* componentRegistry = nullptr;
    std::string name;
    std::optional<std::string>  resourcePath;
    bool dirty = false;
    uint32_t nextId{};

    flecs::world world;
    std::unordered_map<EntityId, flecs::entity, EntityIdHash> entityLookup;

    Rendering::DirectionalLight directionalLight;
    std::vector<Rendering::PointLight> pointLights;

    bool registerEntity(EntityId id, flecs::entity entity);
    void unregisterEntity(EntityId id);

    [[nodiscard]] bool shouldCreateHierarchyCycle(flecs::entity child, flecs::entity newParent) const;
    
    void collectSubtreeEntityIds(flecs::entity entity, std::vector<EntityId>& ids) const;

    [[nodiscard]] std::optional<EntityId> duplicateEntityRecursive(flecs::entity sourceEntity, std::optional<EntityId> parentId); 
    
    void captureRecursive(flecs::entity entity, std::vector<EntitySnapshot>& out) const;

    [[nodiscard]]
    EntityId generateUniqueEntityId() const;
};
} // namespace Cthulhu::Scene