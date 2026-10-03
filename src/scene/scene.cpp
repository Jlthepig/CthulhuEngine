#include "scene.hpp"
#include "light.hpp"
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
    entity.add<TagActive>();

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
    entity.add<TagActive>();

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
    EntityId id = generateEntityId();
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
    if (!duplicate.is_alive())
    {
        return std::nullopt;
    }

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

std::optional<std::vector<EntitySnapshot>> Scene::captureSubtree(EntityId rootId) const
{
    auto root = findEntity(rootId);
    if (!root)
    {
        return std::nullopt;
    }

    std::vector<EntitySnapshot> snapshots;
    captureRecursive(*root, snapshots);
    return snapshots;
}

void Scene::captureRecursive(flecs::entity entity, std::vector<EntitySnapshot> &out) const
{
    const auto *identity = entity.try_get<EntityIdentityComponent>();
    if (!identity)
    {
        return;
    }

    EntitySnapshot snapshot;
    snapshot.id = identity->id;
    snapshot.parentId = getParent(identity->id);

    if (const auto *name = entity.try_get<NameComponent>())
    {
        snapshot.name = name->name;
    }

    if (const auto *transform = entity.try_get<TransformComponent>())
    {
        snapshot.transform = *transform;
    }

    if (const auto *c = entity.try_get<MeshComponent>())
    {
        snapshot.mesh = *c;
    }
    if (const auto *c = entity.try_get<PhysicsComponent>())
    {
        snapshot.physics = *c;
    }
    if (const auto *c = entity.try_get<WeaponComponent>())
    {
        snapshot.weapon = *c;
    }
    if (const auto *c = entity.try_get<AudioSourceComponent>())
    {
        snapshot.audio = *c;
    }
    if (const auto *c = entity.try_get<CharacterControllerComponent>())
    {
        snapshot.characterController = *c;
    }
    if (const auto *c = entity.try_get<CameraComponent>())
    {
        snapshot.camera = *c;
    }
    snapshot.player = entity.has<TagPlayer>();

    out.push_back(std::move(snapshot));

    entity.children([&](flecs::entity child) { captureRecursive(child, out); });
}

bool Scene::restoreSubtree(const std::vector<EntitySnapshot> &snapshots)
{
    std::vector<EntityId> created;

    auto rollback = [&]() {
        for (auto it = created.rbegin(); it != created.rend(); ++it)
        {
            destroyEntity(*it);
        }
    };

    for (const auto &snapshot : snapshots)
    {
        auto entity = createEntityWithId(snapshot.id, snapshot.name);
        if (!entity)
        {
            rollback();
            return false;
        }
        created.push_back(snapshot.id);

        TransformComponent transform = snapshot.transform;
        transform.matrixDirty = true;
        entity->set(transform);

        if (snapshot.mesh)
        {
            entity->set(*snapshot.mesh);
        }
        if (snapshot.physics)
        {
            entity->set(*snapshot.physics);
        }
        if (snapshot.weapon)
        {
            entity->set(*snapshot.weapon);
        }
        if (snapshot.audio)
        {
            entity->set(*snapshot.audio);
        }
        if (snapshot.characterController)
        {
            entity->set(*snapshot.characterController);
        }
        if (snapshot.camera)
        {
            entity->set(*snapshot.camera);
        }
        if (snapshot.player)
        {
            entity->add<TagPlayer>();
        }
    }

    for (const auto &snapshot : snapshots)
    {
        if (snapshot.parentId && !setParent(snapshot.id, *snapshot.parentId))
        {
            rollback();
            return false;
        }
    }

    return true;
}

void Scene::clear()
{
    world.delete_with<TransformComponent>();
    entityLookup.clear();
    
    nextId = 0;

    directionalLight = Rendering::DirectionalLight{};
    pointLights.clear();

    Log::Print("Scene cleared", "Scene", LogType::LOG_INFO);
}
} // namespace Cthulhu::Scene
