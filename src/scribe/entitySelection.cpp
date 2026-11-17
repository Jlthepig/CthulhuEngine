#include "session.hpp"

#include <charconv>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "components.hpp"
#include "engine.hpp"

namespace Cthulhu::Scribe
{
namespace
{
using Scene::EntityId;
using Scene::EntityIdHash;
using Scene::EntitySnapshot;

class EntityBatchCommand final : public Command
{
public:
    EntityBatchCommand(std::vector<EntitySnapshot> values, bool insert, std::string_view title) : snapshots(std::move(values)), inserting(insert), title(title)
    {}

    std::string_view name() const override
    {
        return title;
    }

    Result apply(Context &context) override
    {
        return inserting ? restore(context) : remove(context);
    }

    Result revert(Context &context) override
    {
        return inserting ? remove(context) : restore(context);
    }

private:
    Result restore(Context &context)
    {
        if (!context.scene.restoreSubtree(snapshots))
            return Result::failed("Could not restore the entity selection.");

        for (const auto &snapshot : snapshots)
            context.emit(ChangeType::EntityCreated, snapshot.id);

        return Result::applied();
    }

    Result remove(Context &context)
    {
        auto &scene = context.scene;

        for (const auto &snapshot : snapshots)
        {
            if (!scene.isEntityAlive(snapshot.id) ||
                scene.getParent(snapshot.id) != snapshot.parentId)
            {
                return Result::failed("The entity selection changed.");
            }
        }

        const bool wasDirty = scene.isDirty();

        for (std::size_t i = snapshots.size(); i > 0; --i)
        {
            if (scene.destroyEntity(snapshots[i - 1].id))
                continue;

            const std::vector<EntitySnapshot> removed(snapshots.begin() + i, snapshots.end());

            if (!scene.restoreSubtree(removed))
                return Result::failed("Entity removal failed and could not be rolled back.");

            if (!wasDirty)
                scene.markClean();

            return Result::failed("Could not remove the entity selection.");
        }

        for (auto it = snapshots.rbegin(); it != snapshots.rend(); ++it)
            context.emit(ChangeType::EntityDestroyed, it->id);

        return Result::applied();
    }

    std::vector<EntitySnapshot> snapshots;
    bool inserting;
    std::string title;
};

std::optional<std::string> copyName(std::string_view original, std::unordered_set<std::string> &used)
{
    std::string_view base = original.empty() ? std::string_view{"Entity"} : original;

    std::size_t suffix = 1;
    const auto space = base.rfind(' ');

    if (space != std::string_view::npos && space > 0 && space + 1 < base.size())
    {
        std::size_t previous{};
        const auto digits = base.substr(space + 1);
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), previous);

        if (parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size())
        {
            if (previous == std::numeric_limits<std::size_t>::max())
                return std::nullopt;

            base = base.substr(0, space);
            suffix = previous + 1;
        }
    }

    for (;;)
    {
        std::string candidate = std::string(base) + " " + std::to_string(suffix);

        if (used.insert(candidate).second)
            return candidate;

        if (suffix == std::numeric_limits<std::size_t>::max())
            return std::nullopt;

        ++suffix;
    }
}
}

Result Session::captureEntities(std::span<const EntityId> selection, std::vector<EntitySnapshot> &out)
{
    out.clear();

    if (refuseEdit() || engine.transitionBlocked() || engine.hasTransformOwner())
        return Result::failed("Entity operations require idle Edit mode.");

    const auto *scene = syncScene();
    if (!scene)
        return Result::failed("No active scene.");

    if (selection.empty())
        return Result::noChange();

    std::unordered_set<EntityId, EntityIdHash> selected;

    for (const auto id : selection)
    {
        if (!scene->isEntityAlive(id))
            return Result::failed("A selected entity no longer exists.");

        selected.insert(id);
    }

    std::unordered_set<EntityId, EntityIdHash> captured;

    for (const auto id : selection)
    {
        bool covered = false;

        for (auto parent = scene->getParent(id);
             parent; parent = scene->getParent(*parent))
        {
            if (selected.contains(*parent))
            {
                covered = true;
                break;
            }
        }

        if (covered || !captured.insert(id).second)
            continue;

        auto subtree = scene->captureSubtree(id);
        if (!subtree || subtree->empty())
            return Result::failed("Could not capture the selected entity.");

        out.insert(out.end(),
                   std::make_move_iterator(subtree->begin()),
                   std::make_move_iterator(subtree->end()));
    }

    return Result::applied();
}

Result Session::deleteEntities(std::span<const EntityId> selection)
{
    std::vector<EntitySnapshot> snapshots;
    const auto captured = captureEntities(selection, snapshots);

    if (captured.status != Result::Status::Applied)
        return captured;

    return execute(std::make_unique<EntityBatchCommand>(std::move(snapshots), false, "Delete Entities"));
}

Result Session::copyEntities(std::span<const EntityId> selection, bool cut)
{
    std::vector<EntitySnapshot> snapshots;
    const auto captured = captureEntities(selection, snapshots);

    if (captured.status != Result::Status::Applied)
        return captured;

    const auto *project = engine.getProject();
    if (!project)
        return Result::failed("No open project.");

    const auto projectFile = project->getProjectFilePath();
    const auto generation = engine.getSceneGeneration();

    if (cut)
    {
        const auto removed = execute(std::make_unique<EntityBatchCommand>(snapshots, false, "Cut Entities"));

        if (removed.status != Result::Status::Applied)
            return removed;
    }

    clipboardEntities = std::move(snapshots);
    clipboardGeneration = generation;
    clipboardCut = cut;
    clipboardProject = projectFile;

    return Result::applied();
}

EntitiesResult Session::insertEntities(std::vector<EntitySnapshot> snapshots, std::uint64_t sourceGeneration, std::string_view title, bool keepNames)
{
    if (refuseEdit() || engine.transitionBlocked() || engine.hasTransformOwner())
        return {Result::failed("Entity operations require idle Edit mode."), {}};

    auto *scene = syncScene();
    if (!scene)
        return {Result::failed("No active scene."), {}};

    if (snapshots.empty())
        return {Result::noChange(), {}};

    const bool sameScene = sourceGeneration == engine.getSceneGeneration();

    std::unordered_map<EntityId, EntityId, EntityIdHash> remap;
    std::unordered_set<EntityId, EntityIdHash> generated;

    for (const auto &snapshot : snapshots)
    {
        EntityId id;
        do
        {
            id = Scene::generateEntityId();
        }
        while (!id.isValid() || scene->isEntityAlive(id) || !generated.insert(id).second);

        remap.emplace(snapshot.id, id);
    }

    std::unordered_set<std::string> names;
    scene->getWorld().each([&](const Scene::NameComponent &name) {
        names.insert(name.name);
    });

    std::vector<EntityId> roots;

    for (auto &snapshot : snapshots)
    {
        const auto parent = snapshot.parentId ? remap.find(*snapshot.parentId) : remap.end();

        snapshot.id = remap.at(snapshot.id);

        if (parent != remap.end())
        {
            snapshot.parentId = parent->second;
        }
        else
        {
            roots.push_back(snapshot.id);

            // detached roots keep local transforms add world-space paste when needed.
            if (!sameScene ||
                (snapshot.parentId && !scene->isEntityAlive(*snapshot.parentId)))
            {
                snapshot.parentId.reset();
            }

            snapshot.hasCustomSiblingOrder = false;
            snapshot.siblingOrder = 0;
        }

        if (!keepNames || !names.insert(snapshot.name).second)
        {
            const auto name = copyName(snapshot.name, names);
            if (!name)
                return {
                    Result::failed("Could not allocate a numbered entity name."),
                    {}
                };

            snapshot.name = *name;
        }

        for (auto &component : snapshot.components)
        {
            for (auto &field : component.fields)
            {
                auto *reference = std::get_if<EntityId>(&field.value);
                if (!reference)
                    continue;

                const auto target = remap.find(*reference);

                if (target != remap.end())
                    *reference = target->second;
                else if (!sameScene || !scene->isEntityAlive(*reference))
                    *reference = {};
            }
        }
    }

    auto result = execute(std::make_unique<EntityBatchCommand>(
        std::move(snapshots), true, title));

    if (result.status != Result::Status::Applied)
        roots.clear();

    return {std::move(result), std::move(roots)};
}

EntitiesResult Session::duplicateEntities(std::span<const EntityId> selection)
{
    std::vector<EntitySnapshot> snapshots;
    const auto captured = captureEntities(selection, snapshots);

    if (captured.status != Result::Status::Applied)
        return {captured, {}};

    return insertEntities(std::move(snapshots), engine.getSceneGeneration(), "Duplicate Entities", false);
}

EntitiesResult Session::pasteEntities()
{
    if (clipboardEntities.empty())
        return {Result::noChange(), {}};

    const auto *project = engine.getProject();
    if (!project || project->getProjectFilePath() != clipboardProject)
        return {
            Result::failed("Copy entities from the current project before pasting."),
            {}
        };

    return insertEntities(clipboardEntities, clipboardGeneration, "Paste Entities", clipboardCut);
}
}