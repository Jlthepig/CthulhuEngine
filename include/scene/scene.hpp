#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <flecs.h>

#include "componentRegistry.hpp"
#include "entityId.hpp"
#include "light.hpp"

namespace Cthulhu::Scene
{
struct AssetReference
{
    EntityId entity;
    std::string component;
    std::string field;
    std::string path;
};

struct EntitySnapshot
{
    EntityId id;
    std::optional<EntityId> parentId;
    std::string name;
    std::vector<ComponentSnapshot> components;
};

struct SceneSnapshot
{
    std::string name;
    std::optional<std::string> resourcePath;
    bool dirty = false;
    Rendering::DirectionalLight directionalLight;
    std::vector<Rendering::PointLight> pointLights;
    std::vector<EntitySnapshot> entities;
};
class Scene
{
  public:
    explicit Scene(const ComponentRegistry &registry);

    [[nodiscard]] const ComponentRegistry *getComponentRegistry() const noexcept
    {
        return componentRegistry;
    }

    // << serialization >>
    [[nodiscard]] const std::optional<std::string> &getResourcePath() const noexcept
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
    [[nodiscard]] std::optional<flecs::entity> createEntityWithId(EntityId id, const std::string &name);

    [[nodiscard]] std::optional<flecs::entity> findEntity(EntityId id) const;
    [[nodiscard]] bool isEntityAlive(EntityId id) const;

    // keep in mind names are not unique so it will fetch the first entity with this name
    [[nodiscard]] std::optional<EntityId> findEntityByName(std::string_view name) const;

    [[nodiscard]] std::size_t getEntityCount() const noexcept
    {
        return entityLookup.size();
    }

    bool renameEntity(EntityId id, std::string_view newName);
    bool destroyEntity(EntityId id);
    bool setParent(EntityId childId, EntityId parentId);
    bool clearParent(EntityId childId);

    [[nodiscard]] std::optional<EntityId> getParent(EntityId childId) const;
    [[nodiscard]] std::vector<EntityId> getChildren(EntityId parentId) const;

    [[nodiscard]] std::optional<EntityId> duplicateEntity(EntityId sourceId);

    [[nodiscard]] std::optional<std::vector<EntitySnapshot>> captureSubtree(EntityId rootId) const;

    bool restoreSubtree(const std::vector<EntitySnapshot> &snapshots);

    [[nodiscard]] SceneSnapshot captureScene() const;

    bool restoreScene(const SceneSnapshot& snapshot);

    [[nodiscard]] std::vector<AssetReference> getAssetReferences() const;

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
    const ComponentRegistry *componentRegistry = nullptr;
    std::string name;
    std::optional<std::string> resourcePath;
    bool dirty = false;
    uint32_t nextId{};

    std::unordered_map<EntityId, flecs::entity, EntityIdHash> entityLookup;
    flecs::world world;

    Rendering::DirectionalLight directionalLight;
    std::vector<Rendering::PointLight> pointLights;

    [[nodiscard]] bool shouldCreateHierarchyCycle(flecs::entity child, flecs::entity newParent) const;

    [[nodiscard]] std::optional<EntityId> duplicateEntityRecursive(flecs::entity sourceEntity,
                                                                   std::optional<EntityId> parentId);

    void captureRecursive(flecs::entity entity, std::vector<EntitySnapshot> &out) const;

    [[nodiscard]]
    EntityId generateUniqueEntityId() const;
};
} // namespace Cthulhu::Scene