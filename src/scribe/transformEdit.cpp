#include <algorithm>
#include <utility>

#include "componentCommands.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "session.hpp"

namespace Cthulhu::Scribe
{

Session::~Session()
{
    (void)finishTransformEdit(false);
}

Result Session::beginTransformEdit(std::span<const Scene::EntityId> selection)
{
    if (engine.getTransformOwner() && engine.getTransformOwner() != this)
    {
        return Result::failed("ANOTHER SESSION IS EDITING TRANSFORMS");
    }

    if (refuseEdit())
    {
        return Result::failed("CANNOT EDIT DURING PLAY OR A TRANSFORM PREVIEW");
    }

    auto *scene = syncScene();
    if (!scene || selection.empty())
    {
        return Result::failed("TRANSFORM EDIT NEEDS A SCENE AND SELECTION");
    }

    transformBefore.clear();
    transformBefore.reserve(selection.size());
    for (const auto id : selection)
    {
        const auto entity = scene->findEntity(id);
        const auto *transform = entity ? entity->try_get<Scene::TransformComponent>() : nullptr;
        if (!transform)
        {
            return Result::failed("ENTITY HAS NO TRANSFORM");
        }

        const TransformValue value = captureTransform(id, *transform);
        if (!isFiniteTransform(value))
        {
            return Result::failed("NON-FINITE STARTING TRANSFORM");
        }
        transformBefore.push_back(value);
    }

    std::sort(transformBefore.begin(), transformBefore.end(), [](const TransformValue &a, const TransformValue &b) {
        return a.id.high < b.id.high || (a.id.high == b.id.high && a.id.low < b.id.low);
    });

    const auto duplicate = std::adjacent_find(
        transformBefore.begin(),
        transformBefore.end(),
        [](const TransformValue &a, const TransformValue &b) { return a.id == b.id; });

    if (duplicate != transformBefore.end())
    {
        return Result::failed("DUPLICATE TRANSFORM ENTITY");
    }

    transformAfter = transformBefore;
    transformGeneration = engine.getSceneGeneration();
    transformWasDirty = scene->isDirty();
    transformActive = true;
    engine.setTransformOwner(this);
    history.endMerge();
    return Result::applied();
}

Result Session::previewTransforms(std::span<const TransformValue> values)
{
    auto *scene = syncScene();
    if (!transformActive || !scene)
    {
        return Result::failed("NO ACTIVE TRANSFORM EDIT");
    }
    if (values.size() != transformBefore.size())
    {
        return Result::failed("TRANSFORM SELECTION CHANGED");
    }

    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (values[i].id != transformBefore[i].id || !isFiniteTransform(values[i]))
        {
            return Result::failed("INVALID TRANSFORM PREVIEW");
        }
    }

    Context context{*scene, events};
    const Result result = Commands::writeTransforms(context, values);
    if (!result.ok())
    {
        return result;
    }

    std::copy(values.begin(), values.end(), transformAfter.begin());

    if (transformWasDirty || transformAfter != transformBefore)
    {
        scene->markDirty();
    }
    else
    {
        scene->markClean();
    }
    return result;
}

Result Session::finishTransformEdit(bool commit)
{
    if (!transformActive)
    {
        return Result::noChange();
    }

    auto *scene = syncScene();
    if (!transformActive || !scene)
    {
        return Result::noChange();
    }

    Context context{*scene, events};
    
    const auto &targets = commit ? transformAfter : transformBefore;
    std::vector<TransformValue> survivors;
    for (const auto &v : targets) {
        if (scene->findEntity(v.id)) survivors.push_back(v);
    }
    
    const Result result = Commands::writeTransforms(context, survivors);
    const bool lostTarget = survivors.size() != targets.size();
    const bool changed = commit && !lostTarget && transformBefore != transformAfter;
    transformActive = false;
    if (engine.getTransformOwner() == this)
    {
        engine.setTransformOwner(nullptr);
    }
    history.endMerge();
    if (!result.ok() || lostTarget) {
        transformBefore.clear();
        transformAfter.clear();
        return Result::failed("TRANSFORM TARGET LOST DURING PREVIEW");
    }
    if (changed)
    {
        history.record(Commands::completedTransformEdit(
            std::move(transformBefore), std::move(transformAfter)));
        refreshDirty(*scene);
    }
    else if (transformWasDirty)
    {
        scene->markDirty();
    }
    else
    {
        scene->markClean();
    }

    transformBefore.clear();
    transformAfter.clear();

    return changed ? Result::applied() : Result::noChange();
}
} // namespace Cthulhu::Scribe