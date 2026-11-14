#include "editorShell.hpp"

#include <GLFW/glfw3.h>

#include "engine.hpp"
#include "components.hpp"
#include "input.hpp"
#include "session.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
bool EditorShell::beginAssetDelete(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto request = contentBrowserUI.takeDeleteRequest();
    if (!request.isValid())
        return false;

    const auto *project = engine.getProject();
    const auto *record = engine.getAssetManager().getRegistry().findById(request);

    if (!project || !record || engine.isPlaying() || session.transformEditActive() || assetDrag.asset.isValid() || assetDrag.ownsInput || exitDialog.isOpen(ctx))
    {
        contentBrowserUI.reportError(ctx, "Asset or editing state changed. Try again.");
        return true;
    }

    deletingAsset = request;
    deletingPath = record->path;
    deletingMissing = record->missing;
    deletingProject = project->getProjectFilePath();
    deletingGeneration = engine.getSceneGeneration();

    clearAssetDrag(engine);
    assetPreviewPosition.reset();
    assetFeedback.clear();
    contentBrowserUI.clearFolderDropHighlight(ctx);
    viewportPanelUI.updateMarquee(ctx, sceneNode, false, deletingGeneration);
    session.endMerge();

    if (const auto *window = engine.getWindow(); window && window->getWindow())
    {
        glfwSetInputMode(window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    Core::Input::resetMouseTracking();

    contentBrowserUI.reportError(ctx, "");
    showAssetDelete(ctx, engine, session);
    ctx.clearEvents();
    uiEvents.clear();
    return true;
}

void EditorShell::showAssetDelete(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto usage = session.findAssetUsage(deletingAsset);
    std::vector<std::string> lines{deletingPath};
    const auto entityName = [&](const Scribe::SceneAssetUsage &entry, Scene::EntityId id) -> std::string {
        if (const auto *scene = engine.getActiveScene(); scene &&
            scene->getResourcePath() == entry.scenePath && scene->getName() == entry.sceneName)
        {
            const auto entity = scene->findEntity(id);
            const auto *name = entity ? entity->try_get<Scene::NameComponent>() : nullptr;
            if (name && !name->name.empty())
                return name->name;
        }
        for (const auto &tab : sceneTabs)
        {
            if (!tab.document || tab.document->snapshot.resourcePath != entry.scenePath ||
                tab.document->snapshot.name != entry.sceneName)
                continue;
            for (const auto &entity : tab.document->snapshot.entities)
                if (entity.id == id && !entity.name.empty())
                    return entity.name;
        }
        return "Entity " + Scene::entityIdToString(id);
    };

    for (const auto &entry : usage)
    {
        lines.push_back("Used in " + (entry.sceneName.empty() ? std::string{"Untitled"} : entry.sceneName));

        if (entry.backingFile)
            lines.emplace_back("    This scene is open. Close it before removing its file.");

        for (const auto &reference : entry.references)
        {
            lines.push_back("    " + entityName(entry, reference.entity) + "  (" +
                            reference.component + "." + reference.field + ")");
        }
    }

    exitDialog.show(
        ctx,
        deletingMissing ? "Forget missing asset?" : "Recycle asset?",
        !usage.empty() ? "This asset is still used by an open scene."
                      : deletingMissing
                            ? "Forgetting this asset clears Undo/Redo for all open scenes."
                            : "Recycling clears Undo/Redo for all open scenes.",
        deletingMissing ? "Forget" : "Recycle",
        "");

    exitDialog.setDetails(ctx, lines);
    exitDialog.setNotice(ctx, usage.empty()
        ? "Other saved scenes may also use this asset."
        : "Remove these references or close the scene, then try again.");
    exitDialog.setPrimaryEnabled(ctx, usage.empty());
}

void EditorShell::handleAssetDelete(Context &ctx, Engine &engine,
                                    Scribe::Session &session)
{
    const auto choice = exitDialog.pollChoice(ctx);
    ctx.clearEvents();

    if (choice == DialogChoice::Cancel)
    {
        exitDialog.hide(ctx);
        deletingAsset = {};
        Core::Input::resetMouseTracking();
        return;
    }

    if (choice != DialogChoice::Primary)
        return;

    const auto fail = [&](std::string_view message) {
        exitDialog.setError(ctx, message);
        exitDialog.setPrimaryEnabled(ctx, false);
    };

    const auto *project = engine.getProject();
    const auto *record = engine.getAssetManager().getRegistry().findById(deletingAsset);

    if (!project || project->getProjectFilePath() != deletingProject ||
        engine.getSceneGeneration() != deletingGeneration ||
        engine.isPlaying() || session.transformEditActive() ||
        !record || record->path != deletingPath ||
        record->missing != deletingMissing)
    {
        fail("Asset or scene changed. Cancel, refresh, and try again.");
        return;
    }

    const auto file = project->resolveResourcePath(deletingPath);
    std::error_code error;
    const bool exists = file && std::filesystem::exists(*file, error);

    if (!file || error || exists == deletingMissing || (exists && !std::filesystem::is_regular_file(*file, error)) || error)
    {
        fail("The file changed or is unavailable. Cancel and refresh the browser.");
        return;
    }

    if (!session.findAssetUsage(deletingAsset).empty())
    {
        showAssetDelete(ctx, engine, session);
        return;
    }

    const auto result = session.deleteAsset(deletingAsset);
    if (!result.ok())
    {
        fail(result.error);
        return;
    }

    exitDialog.hide(ctx);
    deletingAsset = {};
    Core::Input::resetMouseTracking();
}
} // namespace Cthulhu::Editor
