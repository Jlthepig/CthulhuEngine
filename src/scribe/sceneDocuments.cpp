#include "session.hpp"

#include <utility>

#include "engine.hpp"

namespace Cthulhu::Scribe
{
bool Session::canSwitchDocument()
{
    syncScene();
    return engine.isInitialized() && engine.getWorldMode() == WorldMode::Edit &&
           !engine.isPlaying() && !transformActive && !engine.hasTransformOwner() &&
           !engine.transitionBlocked();
}

std::optional<SceneDocument> Session::finishDocumentSwitch(std::optional<Scene::SceneSnapshot> previous)
{
    history.endMerge();
    std::optional<SceneDocument> document;
    if (previous)
        document.emplace(SceneDocument{std::move(*previous), std::move(history), &engine});

    history = History{};
    boundGeneration = engine.getSceneGeneration();
    std::erase_if(events, [](const ChangeEvent &event) { return !isAssetChange(event.type); });
    events.push_back({ChangeType::SceneReplaced});
    return document;
}

DocumentSwitchResult Session::openSceneDocument(std::string_view resourcePath)
{
    if (!canSwitchDocument())
        return {Result::failed("SCENE SWITCH REQUIRES EDIT MODE AT A FRAME BOUNDARY"), {}};

    const auto *active = engine.getActiveScene();
    auto previous = active ? std::optional{active->captureScene()} : std::nullopt;
    if (!engine.loadScene(resourcePath))
        return {Result::failed("COULD NOT OPEN SCENE: " + std::string(resourcePath)), {}};

    return {Result::applied(), finishDocumentSwitch(std::move(previous))};
}

DocumentSwitchResult Session::activateSceneDocument(std::optional<SceneDocument> &target)
{
    if (!canSwitchDocument())
        return {Result::failed("SCENE SWITCH REQUIRES EDIT MODE AT A FRAME BOUNDARY"), {}};

    if (!target || target->owner != &engine)
        return {Result::failed("NO INACTIVE SCENE FOR THIS ENGINE"), {}};

    const auto *active = engine.getActiveScene();
    auto previous = active ? std::optional{active->captureScene()} : std::nullopt;
    if (!engine.restoreEditScene(target->snapshot))
        return {Result::failed("COULD NOT RESTORE SCENE: " + target->snapshot.name), {}};

    auto parked = finishDocumentSwitch(std::move(previous));
    history = std::move(target->history);
    target.reset();
    return {Result::applied(), std::move(parked)};
}
} // namespace Cthulhu::Scribe