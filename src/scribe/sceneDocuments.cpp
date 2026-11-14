#include "session.hpp"

#include <utility>
#include <algorithm>

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

SceneDocument *Session::finishDocumentSwitch(std::optional<Scene::SceneSnapshot> previous)
{
    history.endMerge();
    SceneDocument *document = nullptr;
    if (previous)
        document = &inactiveDocuments.emplace_back(SceneDocument{std::move(*previous), std::move(history)});

    history = History{};
    boundGeneration = engine.getSceneGeneration();
    std::erase_if(events, [](const ChangeEvent &event) { return !isAssetChange(event.type); });
    events.push_back({ChangeType::SceneReplaced});
    return document;
}

DocumentSwitchResult Session::newSceneDocument()
{
    if (!canSwitchDocument())
        return {Result::failed("Scene creation requires Edit mode at a frame boundary."), {}};

    const auto *active = engine.getActiveScene();
    auto previous = active ? std::optional{active->captureScene()} : std::nullopt;

    if (!engine.createEmptyScene("Untitled"))
        return {Result::failed("Could not create an empty scene."), {}};

    return {Result::applied(),finishDocumentSwitch(std::move(previous))};
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

DocumentSwitchResult Session::activateSceneDocument(SceneDocument *&target, bool keepCurrent)
{
    if (!canSwitchDocument())
        return {Result::failed("SCENE SWITCH REQUIRES EDIT MODE AT A FRAME BOUNDARY"), {}};

    const auto document = std::find_if(inactiveDocuments.begin(), inactiveDocuments.end(),
                                       [target](const SceneDocument &candidate) { return &candidate == target; });
    if (document == inactiveDocuments.end())
        return {Result::failed("NO INACTIVE SCENE FOR THIS SESSION"), {}};

    const auto *active = engine.getActiveScene();
    auto previous = keepCurrent && active ? std::optional{active->captureScene()} : std::nullopt;
    if (!engine.restoreEditScene(target->snapshot))
        return {Result::failed("COULD NOT RESTORE SCENE: " + target->snapshot.name), {}};

    auto *parked = finishDocumentSwitch(std::move(previous));
    history = std::move(document->history);
    inactiveDocuments.erase(document);
    target = nullptr;
    return {Result::applied(), parked};
}

Result Session::closeActiveSceneDocument()
{
    if (!canSwitchDocument())
        return Result::failed("SCENE CLOSE REQUIRES EDIT MODE AT A FRAME BOUNDARY");
    if (!engine.hasActiveScene())
        return Result::noChange();

    engine.unloadScene();
    if (engine.hasActiveScene())
        return Result::failed("COULD NOT CLOSE ACTIVE SCENE");

    finishDocumentSwitch(std::nullopt);
    return Result::applied();
}

Result Session::closeInactiveSceneDocument(SceneDocument *&target)
{
    if (!canSwitchDocument())
        return Result::failed("SCENE CLOSE REQUIRES EDIT MODE AT A FRAME BOUNDARY");

    const auto document = std::find_if(inactiveDocuments.begin(), inactiveDocuments.end(),
                                       [target](const SceneDocument &candidate) { return &candidate == target; });
    if (document == inactiveDocuments.end())
        return Result::failed("NO INACTIVE SCENE FOR THIS SESSION");

    inactiveDocuments.erase(document);
    target = nullptr;
    return Result::applied();
}
} // namespace Cthulhu::Scribe