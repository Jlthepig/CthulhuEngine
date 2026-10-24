#include <algorithm>
#include <iterator>
#include <unordered_set>

#include "scene.hpp"
#include "assetManager.hpp"
#include "components.hpp"
#include "light.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{

namespace
{
void refreshSpatialRuntime(flecs::entity entity)
{
    if (const auto *physics = entity.try_get<PhysicsComponent>())
    {
        const PhysicsComponent refreshed = *physics;
        entity.set(refreshed);
    }
    if (const auto *character = entity.try_get<CharacterControllerComponent>())
    {
        const CharacterControllerComponent refreshed = *character;
        entity.set(refreshed);
    }
}
} // namespace

Scene::Scene(const ComponentRegistry& registry) : componentRegistry(&registry)
{
    world.observer<const EntityIdentityComponent>("EntityLookupAdd")
        .event(flecs::OnSet)
        .each([this](flecs::entity entity, const EntityIdentityComponent& identity) {
            auto [it, inserted] = entityLookup.try_emplace(identity.id, entity);
            if (!inserted && it->second != entity && it->second.is_alive())
            {
                Log::Print("DUPLICATE ENTITY ID: " + entityIdToString(identity.id), "Scene", LogType::LOG_ERROR);
                return;
            }
            it->second = entity;
        });

    world.observer<const EntityIdentityComponent>("EntityLookupRemove")
        .event(flecs::OnRemove)
        .each([this](flecs::entity entity, const EntityIdentityComponent& identity) {
            auto it = entityLookup.find(identity.id);
            if (it != entityLookup.end() && it->second == entity)
            {
                removeEntityFromSiblingOrders(identity.id);
                entityLookup.erase(it);
            }
        });
}

// << entity management >>
flecs::entity Scene::createEntity(const std::string &name)
{
    const EntityId id = generateUniqueEntityId();

    auto entity = world.entity();

    entity.set<EntityIdentityComponent>({id});
    entity.set<NameComponent>({name.empty() ? "Entity" : name});
    entity.set<TransformComponent>({});
    entity.add<TagActive>();
    appendToCustomSiblingGroup({}, id);

    markDirty();
    return entity;
}

std::optional<flecs::entity> Scene::createEntityWithId(EntityId id, const std::string &name)
{
    if (!id.isValid())
    {
        return std::nullopt;
    }

    if (entityLookup.contains(id))
    {
        Log::Print("ATTEMPTED TO CREATE ENTITY WITH DUPLICATE ID: " + entityIdToString(id), "Scene",
                   LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto entity = world.entity();

    entity.set<EntityIdentityComponent>({id});
    entity.set<NameComponent>({name.empty() ? "Entity" : name});
    entity.set(TransformComponent{});
    entity.add<TagActive>();
    appendToCustomSiblingGroup({}, id);

    return entity;
}

bool Scene::destroyEntity(EntityId id)
{
    auto entity = findEntity(id);
    if (!entity)
    {
        return false;
    }

    entity->destruct();
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

    if (!it->second || !it->second.is_alive())
    {
        return std::nullopt;
    }
    return it->second;
}

bool Scene::isEntityAlive(EntityId id) const
{
    return findEntity(id).has_value();
}

std::optional<EntityId> Scene::findEntityByName(std::string_view name) const
{
    for (const auto& [id, entity] : entityLookup)
    {
        const auto* entityName = entity.try_get<NameComponent>();
        if (entityName && entityName->name == name)
        {
            return id;
        }
    }
    return std::nullopt;
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

    const std::optional<EntityId> oldParentId = getParent(childId);
    if (oldParentId && *oldParentId == parentId)
    {
        return true;
    }

    if (shouldCreateHierarchyCycle(*child, *parent))
    {
        Log::Print("FAILED TO SET AN ENTITY PARENT: HIERARCHY CYCLE", "Scene", LogType::LOG_ERROR);
        return false;
    }

    if (oldParentId)
    {
        removeFromCustomSiblingGroup(*oldParentId, childId);
    }
    else
    {
        removeFromCustomSiblingGroup({}, childId);
    }

    child->child_of(*parent);
    appendToCustomSiblingGroup(parentId, childId);
    refreshSpatialRuntime(*child);
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

    const std::optional<EntityId> oldParentId = getParent(childId);
    if (!oldParentId)
    {
        return true;
    }

    auto parent = child->parent();
    removeFromCustomSiblingGroup(*oldParentId, childId);
    child->remove(flecs::ChildOf, parent);
    appendToCustomSiblingGroup({}, childId);
    refreshSpatialRuntime(*child);
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

    if (!parent || !parent.is_alive())
    {
        return std::nullopt;
    }

    if (!parent.has<EntityIdentityComponent>())
    {
        return std::nullopt;
    }

    return parent.get<EntityIdentityComponent>().id;
}

void Scene::sortSiblings(std::vector<EntityId>& siblings, EntityId parentKey) const
{
    auto byName = [this](EntityId left, EntityId right) {
        const auto leftEntity = findEntity(left);
        const auto rightEntity = findEntity(right);
        const auto* leftName = leftEntity ? leftEntity->try_get<NameComponent>() : nullptr;
        const auto* rightName = rightEntity ? rightEntity->try_get<NameComponent>() : nullptr;
        const std::string_view leftText = leftName ? leftName->name : std::string_view{};
        const std::string_view rightText = rightName ? rightName->name : std::string_view{};

        if (leftText != rightText)
        {
            return leftText < rightText;
        }
        if (left.high != right.high)
        {
            return left.high < right.high;
        }
        return left.low < right.low;
    };

    const auto customOrder = customSiblingOrders.find(parentKey);
    if (customOrder == customSiblingOrders.end())
    {
        std::sort(siblings.begin(), siblings.end(), byName);
        return;
    }

    std::unordered_set<EntityId, EntityIdHash> remaining(siblings.begin(), siblings.end());
    std::vector<EntityId> ordered;
    ordered.reserve(siblings.size());

    for (const EntityId id : customOrder->second)
    {
        if (remaining.erase(id) != 0)
        {
            ordered.push_back(id);
        }
    }

    std::vector<EntityId> unlisted(remaining.begin(), remaining.end());
    std::sort(unlisted.begin(), unlisted.end(), byName);
    ordered.insert(ordered.end(), unlisted.begin(), unlisted.end());
    siblings = std::move(ordered);
}

void Scene::appendToCustomSiblingGroup(EntityId parentKey, EntityId childId)
{
    auto order = customSiblingOrders.find(parentKey);
    if (order != customSiblingOrders.end() &&
        std::find(order->second.begin(), order->second.end(), childId) == order->second.end())
    {
        order->second.push_back(childId);
    }
}

void Scene::removeFromCustomSiblingGroup(EntityId parentKey, EntityId childId)
{
    auto order = customSiblingOrders.find(parentKey);
    if (order == customSiblingOrders.end())
    {
        return;
    }

    auto& siblings = order->second;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), childId), siblings.end());
    if (siblings.empty())
    {
        customSiblingOrders.erase(order);
    }
}

void Scene::removeEntityFromSiblingOrders(EntityId id)
{
    customSiblingOrders.erase(id);

    for (auto order = customSiblingOrders.begin(); order != customSiblingOrders.end();)
    {
        auto& siblings = order->second;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        if (siblings.empty())
        {
            order = customSiblingOrders.erase(order);
        }
        else
        {
            ++order;
        }
    }
}

std::vector<EntityId> Scene::getRootEntities() const
{
    std::vector<EntityId> roots;
    roots.reserve(entityLookup.size());

    for (const auto& [id, entity] : entityLookup)
    {
        if (id.isValid() && entity && entity.is_alive() && !getParent(id))
        {
            roots.push_back(id);
        }
    }

    sortSiblings(roots, {});
    return roots;
}

std::vector<EntityId> Scene::getChildren(EntityId parentId) const
{
    std::vector<EntityId> children;
    auto parent = findEntity(parentId);
    if (!parent)
    {
        return children;
    }

    parent->children([&](flecs::entity child) {
        if (child.has<EntityIdentityComponent>())
        {
            children.push_back(child.get<EntityIdentityComponent>().id);
        }
    });

    sortSiblings(children, parentId);
    return children;
}

std::optional<std::size_t> Scene::getSiblingOrder(EntityId childId) const
{
    const auto parentId = getParent(childId);
    const std::vector<EntityId> siblings = parentId ? getChildren(*parentId) : getRootEntities();
    const auto position = std::find(siblings.begin(), siblings.end(), childId);
    if (position == siblings.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(siblings.begin(), position));
}

bool Scene::hasCustomSiblingOrder(std::optional<EntityId> parentId) const
{
    return customSiblingOrders.contains(parentId.value_or(EntityId{}));
}

bool Scene::setSiblingOrder(EntityId childId, std::size_t order)
{
    auto siblingsParent = getParent(childId);
    std::vector<EntityId> siblings = siblingsParent ? getChildren(*siblingsParent) : getRootEntities();
    const auto current = std::find(siblings.begin(), siblings.end(), childId);
    if (current == siblings.end() || order >= siblings.size())
    {
        return false;
    }

    const bool wasCustom = hasCustomSiblingOrder(siblingsParent);
    const std::size_t oldOrder = static_cast<std::size_t>(std::distance(siblings.begin(), current));
    if (!wasCustom && oldOrder == order)
    {
        customSiblingOrders.emplace(siblingsParent.value_or(EntityId{}), siblings);
        markDirty();
        return true;
    }

    if (oldOrder != order)
    {
        const EntityId moving = *current;
        siblings.erase(current);
        siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(order), moving);
    }

    if (!wasCustom || oldOrder != order)
    {
        customSiblingOrders[siblingsParent.value_or(EntityId{})] = std::move(siblings);
        markDirty();
    }

    return true;
}

bool Scene::clearCustomSiblingOrder(std::optional<EntityId> parentId)
{
    const auto order = customSiblingOrders.find(parentId.value_or(EntityId{}));
    if (order == customSiblingOrders.end())
    {
        return false;
    }

    customSiblingOrders.erase(order);
    markDirty();
    return true;
}

std::optional<EntityId> Scene::duplicateEntityRecursive(flecs::entity source, std::optional<EntityId> parentId)
{
    if (!source.is_alive())
    {
        return std::nullopt;
    }

    const auto *sourceIdentity = source.try_get<EntityIdentityComponent>();
    if (!sourceIdentity)
    {
        return std::nullopt;
    }

    const auto *sourceName = source.try_get<NameComponent>();
    const std::string name = sourceName ? sourceName->name : "Entity";

    flecs::entity duplicate = createEntity(name);
    if (!duplicate.is_alive())
    {
        return std::nullopt;
    }

    const auto *identity = duplicate.try_get<EntityIdentityComponent>();

    if (!identity)
    {
        duplicate.destruct();
        return std::nullopt;
    }

    const EntityId duplicateId = identity->id;

    if (!componentRegistry->apply(duplicate, componentRegistry->capture(source)))
    {
        destroyEntity(duplicateId);
        return std::nullopt;
    }

    if (parentId)
    {
        if (!setParent(duplicateId, *parentId))
        {
            destroyEntity(duplicateId);
            return std::nullopt;
        }
    }

    std::vector<EntityId> duplicatedChildren;
    const std::vector<EntityId> sourceChildren = getChildren(sourceIdentity->id);
    for (const EntityId sourceChildId : sourceChildren)
    {
        auto sourceChild = findEntity(sourceChildId);
        if (!sourceChild)
        {
            Log::Print("ENTITY HIERARCHY CHILD IS NO LONGER ALIVE", "Scene", LogType::LOG_ERROR);
            destroyEntity(duplicateId);
            return std::nullopt;
        }

        const auto childDuplicate = duplicateEntityRecursive(*sourceChild, duplicateId);
        if (!childDuplicate)
        {
            destroyEntity(duplicateId);
            return std::nullopt;
        }
        duplicatedChildren.push_back(*childDuplicate);
    }

    if (hasCustomSiblingOrder(sourceIdentity->id))
    {
        for (std::size_t index = 0; index < duplicatedChildren.size(); ++index)
        {
            if (!setSiblingOrder(duplicatedChildren[index], index))
            {
                destroyEntity(duplicateId);
                return std::nullopt;
            }
        }
    }

    return duplicateId;
}

std::optional<EntityId> Scene::duplicateEntity(EntityId sourceId)
{
    if (world.is_deferred())
    {
        Log::Print("DUPLICATE CANNOT RUN INSIDE A SYSTEM", "Scene", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto source = findEntity(sourceId);

    if (!source)
    {
        return std::nullopt;
    }

    const bool wasDirty = isDirty();
    const std::optional<EntityId> parentId = getParent(sourceId);

    auto duplicateId = duplicateEntityRecursive(*source, parentId);

    if (!duplicateId)
    {
        if (!wasDirty)
        {
            markClean();
        }

        Log::Print("FAILED TO DUPLICATE ENTITY", "Scene", LogType::LOG_ERROR);
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
    snapshot.hasCustomSiblingOrder = hasCustomSiblingOrder(snapshot.parentId);
    snapshot.siblingOrder = getSiblingOrder(snapshot.id).value_or(0);

    if (const auto *name = entity.try_get<NameComponent>())
    {
        snapshot.name = name->name;
    }

    snapshot.components = componentRegistry->capture(entity);

    out.push_back(std::move(snapshot));

    for (const EntityId childId : getChildren(identity->id))
    {
        if (auto child = findEntity(childId))
        {
            captureRecursive(*child, out);
        }
    }
}

std::vector<AssetReference> Scene::getAssetReferences() const
{
    std::vector<AssetReference> out;

    for (const auto &[id, entity] : entityLookup)
    {
        for (const auto &descriptor : componentRegistry->getAll())
        {
            if (!descriptor.has(entity))
            {
                continue;
            }

            for (const auto &field : descriptor.fields)
            {
                if (field.type != FieldType::AssetRef)
                {
                    continue;
                }

                std::string path = std::get<std::string>(field.get(entity));
                if (!path.empty())
                {
                    out.push_back({id, descriptor.name, field.name, std::move(path)});
                }
            }
        }
    }

    return out;
}

std::vector<AssetReference> Scene::findAssetReferences(std::string_view resourcePath) const
{

    auto keyOf = [](std::string_view path) { return Assets::normaliseResourcePath(path).value_or(std::string(path)); };

    const std::string target = keyOf(resourcePath);

    std::vector<AssetReference> out;
    for (auto &ref : getAssetReferences())
    {
        if (keyOf(ref.path) == target)
        {
            out.push_back(std::move(ref));
        }
    }

    return out;
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

        if (!componentRegistry->apply(*entity, snapshot.components))
        {
            rollback();
            return false;
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

    for (const auto& snapshot : snapshots)
    {
        if (snapshot.hasCustomSiblingOrder && !setSiblingOrder(snapshot.id, snapshot.siblingOrder))
        {
            rollback();
            return false;
        }
    }

    return true;
}

SceneSnapshot Scene::captureScene() const
{
    SceneSnapshot snapshot;
    snapshot.name = name;
    snapshot.resourcePath = resourcePath;
    snapshot.dirty = dirty;
    snapshot.directionalLight = directionalLight;
    snapshot.pointLights = pointLights;

    for (const EntityId id : getRootEntities())
    {
        if (auto subtree = captureSubtree(id))
        {
            for (auto &entitySnapshot : *subtree)
            {
                snapshot.entities.push_back(std::move(entitySnapshot));
            }
        }
    }

    return snapshot;
}

bool Scene::restoreScene(const SceneSnapshot& snapshot)
{
    if (!entityLookup.empty())
    {
        return false;
    }

    if (!restoreSubtree(snapshot.entities))
    {
        return false;
    }

    name = snapshot.name;
    resourcePath = snapshot.resourcePath;
    directionalLight = snapshot.directionalLight;
    pointLights = snapshot.pointLights;

    dirty = snapshot.dirty; 
    return true;
}

void Scene::clear()
{
    world.delete_with<TransformComponent>();
    entityLookup.clear();
    customSiblingOrders.clear();

    nextId = 0;

    directionalLight = Rendering::DirectionalLight{};
    pointLights.clear();

    Log::Print("Scene cleared", "Scene", LogType::LOG_INFO);
}
} // namespace Cthulhu::Scene
