#include "scene.hpp"
#include "light.hpp"
#include "modelLoader.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{

namespace 
{

template <typename T, typename CopyFn>
void copyIfPresent(flecs::entity source, flecs::entity destination, CopyFn&& copyFn)
{
    if (const auto* src = source.try_get<T>())
    {
        T dst{};
        copyFn(*src, dst);
        destination.set(dst);
    }
}

} // namespace

// << entity management >>
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

std::optional<flecs::entity> Scene::createEntityWithId(EntityId id,const std::string& name)
{
    if (!id.isValid())
    {
        return std::nullopt;
    }

    if (entityLookup.contains(id))
    {
        Log::Print("ATTEMPTED TO CREATE ENTITY WITH DUPLICATE ID: " + entityIdToString(id),"Scene",LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto entity =world.entity();

    entity.set<EntityIdentityComponent>({id});
    entity.set<NameComponent>({name.empty() ? "Entity" : name});
    entity.set(TransformComponent{});

    if (!registerEntity(id,entity))
    {
        entity.destruct();
        return std::nullopt;
    }

    return entity;
}

bool Scene::destroyEntity(EntityId id)
{
    auto entity = findEntity(id);
    if (!entity)
    {
        return false;
    }

    std::vector<EntityId> subtreeIds;

    collectSubtreeEntityIds(*entity,subtreeIds);
    entity->destruct();

    for (const EntityId subtreeId : subtreeIds)
    {
        entityLookup.erase(subtreeId);
    }

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

bool Scene::shouldCreateHierarchyCycle(flecs::entity child, flecs::entity newParent) const
{
    if (child == newParent)
    {
        return true;
    }

    auto current = newParent;
    while (current.is_alive())
    {
        if (current == child)
        {
            return true;
        }

        current = current.parent();
    }

    return false;
}

bool Scene::setParent(EntityId childId, EntityId parentId)
{
    auto child = findEntity(childId);
    auto parent = findEntity(parentId);

    if (!child || !parent)
    {
        return false;
    }

    if (childId == parentId)
    {
        return false;
    }

    auto currentParent = child->parent();

    if (currentParent.is_alive() && currentParent == *parent)
    {
        return true;
    }

    if (shouldCreateHierarchyCycle(*child, *parent))
    {
        Log::Print("FAILED TO SET AN ENTITY PARENT: HIERARCHY CYCLE", "Scene", LogType::LOG_ERROR);
        return false;
    }

    child->child_of(*parent);
    markDirty();
    return true;
}

bool Scene::clearParent(EntityId childId)
{
    auto child = findEntity(childId);

    if (!child)
    {
        return false;
    }

    auto parent = child->parent();

    if (!parent.is_alive())
    {
        return true;
    }

    child->remove(flecs::ChildOf,parent);
    markDirty();
    return true;
}

std::optional<EntityId> Scene::getParent(EntityId childId) const
{
    auto child = findEntity(childId);

    if (!child)
    {
        return std::nullopt;
    }

    auto parent = child->parent();

    if (!parent.is_alive())
    {
        return std::nullopt;
    }

    if (!parent.has<EntityIdentityComponent>())
    {
        return std::nullopt;
    }

    return parent.get<EntityIdentityComponent>().id;
}

std::vector<EntityId> Scene::getChildren(EntityId parentId) const
{
    std::vector<EntityId> children;

    auto parent = findEntity(parentId);

    if (!parent)
    {
        return children;
    }

    parent->children([&](flecs::entity child)
    {
        if (!child.has<EntityIdentityComponent>())
        {
            return;
        }

        children.push_back(child.get<EntityIdentityComponent>().id);
    });

    return children;
}

void Scene::collectSubtreeEntityIds(flecs::entity entity,std::vector<EntityId>& ids) const
{
    entity.children([&](flecs::entity child)
    {
        collectSubtreeEntityIds(child,ids);
    });

    if (entity.has<EntityIdentityComponent>())
    {
        ids.push_back(entity.get<EntityIdentityComponent>().id);
    }
}

void Scene::copyAuthoringComponents(flecs::entity source,flecs::entity destination)
{
    copyIfPresent<TransformComponent>(source, destination,[](const TransformComponent& src, TransformComponent& dst)
    {
        dst.position = src.position;
        dst.rotation = src.rotation;
        dst.scale = src.scale;
        dst.matrixDirty = true;
    });

    if (const auto* component = source.try_get<MeshComponent>())
    {
        destination.set(*component);

        if (const auto* sourceRuntime = source.try_get<MeshRuntimeComponent>())
        {
            MeshRuntimeComponent runtime;
            runtime.model = sourceRuntime->model;
            destination.set(runtime);
        }
    }

    if (const auto* component = source.try_get<PhysicsComponent>())
    {
        destination.set(*component);
    }

    if (const auto* component = source.try_get<WeaponComponent>())
    {
        destination.set(*component);
    }

    if (const auto* component = source.try_get<AudioSourceComponent>())
    {
        destination.set(*component);
    }

    if (const auto* component = source.try_get<CharacterControllerComponent>())
    {
        destination.set(*component);
    }

    copyIfPresent<CameraComponent>(source, destination,[](const CameraComponent& src, CameraComponent& dst)
    {
        dst.front = src.front;
    });

    if (source.has<TagPlayer>())
    {
        destination.add<TagPlayer>();
    }
}

std::optional<EntityId> Scene::duplicateEntityRecursive(flecs::entity source,std::optional<EntityId> parentId)
{
    if (!source.is_alive())
    {
        return std::nullopt;
    }

    const auto* sourceName =source.try_get<NameComponent>();

    const std::string name = sourceName ? sourceName->name : "Entity";

    flecs::entity duplicate = createEntity(name);

    const auto* identity = duplicate.try_get<EntityIdentityComponent>();

    if (!identity)
    {
        duplicate.destruct();
        return std::nullopt;
    }

    const EntityId duplicateId = identity->id;

    copyAuthoringComponents(source, duplicate);

    if (parentId)
    {
        if (!setParent(duplicateId,*parentId))
        {
            destroyEntity(duplicateId);
            return std::nullopt;
        }
    }

    bool success = true;

    source.children([&](flecs::entity sourceChild)
        {
            if (!success)
            {
                return;
            }

            if (!sourceChild.has<EntityIdentityComponent>())
            {
                Log::Print("ENTITY HIERARCHY CHILD HAS NO ENTITY ID","Scene",LogType::LOG_ERROR);
                success = false;
                return;
            }

            if (!duplicateEntityRecursive(sourceChild,duplicateId))
            {
                success = false;
            }
        });

    if (!success)
    {
        destroyEntity(duplicateId);
        return std::nullopt;
    }

    return duplicateId;
}

std::optional<EntityId> Scene::duplicateEntity(EntityId sourceId)
{
    auto source = findEntity(sourceId);

    if (!source)
    {
        return std::nullopt;
    }

    const bool wasDirty =isDirty();
    const std::optional<EntityId> parentId = getParent(sourceId);

    auto duplicateId = duplicateEntityRecursive(*source,parentId);

    if (!duplicateId)
    {
        if (!wasDirty)
        {
            markClean();
        }

        Log::Print("FAILED TO DUPLICATE ENTITY","Scene",LogType::LOG_ERROR);
        return std::nullopt;
    }

    markDirty();
    return duplicateId;
}

// << asset lighting >> 
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
