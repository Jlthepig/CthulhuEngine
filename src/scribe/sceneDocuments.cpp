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

MainScenePlayResult Session::playMainScene()
{
    if (!canSwitchDocument() || !engine.hasActiveScene())
        return {Result::failed("Play requires an active scene in Edit mode.")};

    const auto *project = engine.getProject();
    if (!project)
        return {Result::failed("Open a project before starting Play.")};
    if (!project->hasMainScene())
        return {Result::failed("Choose a saved scene to use as the project's main scene."), true};

    const auto &registry = engine.getAssetManager().getRegistry();
    const auto *diskPath = project->getMainScene(registry);
    const auto *sourcePath = diskPath;
    if (!sourcePath && project->getConfig().mainSceneId)
    {
        // a missing file can still have an open edit document
        const auto *record = registry.findById(*project->getConfig().mainSceneId);
        if (record && record->type == Assets::AssetType::Scene)
            sourcePath = &record->path;
    }

    constexpr auto unavailable = "The assigned main scene is unavailable. Choose a saved scene.";
    if (!sourcePath)
        return {Result::failed(unavailable), true};

    const auto key = Assets::normaliseResourcePath(*sourcePath);
    const auto file = project->resolveResourcePath(*sourcePath);
    const auto matches = [&](const std::optional<std::string> &path) {
        if (!path)
            return false;
        if (*path == *sourcePath || (key && Assets::normaliseResourcePath(*path) == key))
            return true;

        const auto candidate = project->resolveResourcePath(*path);
        std::error_code error;
        return file && candidate && std::filesystem::equivalent(*file, *candidate, error) && !error;
    };

    if (matches(engine.getActiveScene()->getResourcePath()))
        return {engine.play() ? Result::applied() : Result::failed("Could not play the open main scene.")};

    for (const auto &document : inactiveDocuments)
    {
        if (matches(document.snapshot.resourcePath))
        {
            return {engine.playFromSnapshot(document.snapshot) ? Result::applied() : Result::failed("Could not play the open main scene.")};
        }
    }

    if (diskPath && engine.playFromFile(*diskPath))
        return {Result::applied()};

    std::error_code error;
    if (!diskPath || !file || !std::filesystem::is_regular_file(*file, error))
        return {Result::failed(unavailable), true};

    return {Result::failed("Could not load the saved main scene. Check Project Settings and the log.")};
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