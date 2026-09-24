#include "scene.hpp"
#include "light.hpp"
#include "modelLoader.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
flecs::entity Scene::createEntity(const std::string &name)
{
    const EntityId id = generateUniqueEntityId();

    auto entity = world.entity();

    entity.set<EntityIdentityComponent>({id});
    entity.set<NameComponent>({name.empty() ? "Entity" : name});
    entity.set<TransformComponent>({});

    if (!registerEntity(id, entity))
    {
        entity.destruct();
        Log::Print("FAILED TO REGISTER ENTITY", "Scene", LogType::LOG_ERROR);
        return flecs::entity::null();
    }

    markDirty();
    return entity;
}

bool Scene::destroyEntity(EntityId id)
{
    auto it = entityLookup.find(id);

    if (it == entityLookup.end())
    {
        return false;
    }

    flecs::entity entity = it->second;

    if (!entity.is_alive())
    {
        entityLookup.erase(it);
        return false;
    }

    entity.destruct();
    entityLookup.erase(it);

    markDirty();
    return true;
}

std::optional<flecs::entity> Scene::findEntity(EntityId id) const
{
    if (!id.isValid())
    {
        return std::nullopt;
    }

    auto it = entityLookup.find(id);

    if (it == entityLookup.end())
    {
        return std::nullopt;
    }

    if (!it->second.is_alive())
    {
        return std::nullopt;
    }
    return it->second;
}

bool Scene::isEntityAlive(EntityId id) const
{
    return findEntity(id).has_value();
}

bool Scene::renameEntity(EntityId id, std::string_view newName)
{
    if (newName.empty())
    {
        return false;
    }

    auto entity = findEntity(id);

    if (!entity)
    {
        return false;
    }

    entity->set<NameComponent>({std::string(newName)});

    markDirty();
    return true;
}

EntityId Scene::generateUniqueEntityId() const
{
    EntityId id;
    while (entityLookup.contains(id))
    {
        id = generateEntityId();
    }
    return id;
}

bool Scene::registerEntity(EntityId id, flecs::entity entity)
{
    if (!id.isValid() || !entity.is_alive())
    {
        return false;
    }

    if (entityLookup.contains(id))
    {
        return false;
    }

    entityLookup.emplace(id, entity);
    return true;
}

void Scene::unregisterEntity(EntityId id)
{
    entityLookup.erase(id);
}

Rendering::Model *Scene::getOrLoadModel(const std::string &resourcePath, const std::filesystem::path &fileSystemPath)
{
    auto it = modelCache.find(resourcePath);
    if (it != modelCache.end())
    {
        Log::Print("Resuing model from cache: " + resourcePath, "Scene", LogType::LOG_INFO);
        return &it->second;
    }
    else
    {
        Log::Print("Loading model from file: " + resourcePath, "Scene", LogType::LOG_INFO);
        Rendering::Model model = Rendering::ModelLoader::loadGltf(fileSystemPath.string());
        modelCache[resourcePath] = std::move(model);
        return &modelCache[resourcePath];
    }
}

void Scene::clear()
{
    world.delete_with<TransformComponent>();
    entityLookup.clear();
    
    for (auto &[path, model] : modelCache)
    {
        model.destroy();
    }
    modelCache.clear();
    nextId = 0;

    directionalLight = Rendering::DirectionalLight{};
    pointLights.clear();

    Log::Print("Scene cleared", "Scene", LogType::LOG_INFO);
}
} // namespace Cthulhu::Scene
