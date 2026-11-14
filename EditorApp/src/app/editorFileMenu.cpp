#include <GLFW/glfw3.h>

#include "editorShell.hpp"

#include <cassert>

#include "engine.hpp"
#include "input.hpp"
#include "session.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
namespace
{
enum FileAction : u64
{
    NewScene = 1, OpenScene, OpenRecent, SaveScene, SaveSceneAs,
    CloseScene, CloseAllScenes, Quit, RecentScene = 100
};
}

Scribe::Result EditorShell::newSceneTab(Context &ctx, Engine &engine, Scribe::Session &session)
{
    if (engine.isPlaying() || session.transformEditActive())
        return Scribe::Result::failed("Scene creation requires Edit mode.");

    const auto *scene = engine.getActiveScene();
    for (std::size_t i = 0; i < sceneTabs.size(); ++i)
    {
        const auto *document = sceneTabs[i].document;
        const bool empty = document
            ? !document->snapshot.resourcePath &&
                !document->snapshot.dirty &&
                document->snapshot.entities.empty()
            : scene && !scene->hasResourcePath() && !scene->isDirty() &&
                scene->getEntityCount() == 0;

        if (empty)
            return selectSceneTab(engine, session, i);
    }

    rememberSceneView(engine);
    auto created = session.newSceneDocument();
    if (!created.result.ok())
        return created.result;

    if (created.previous)
    {
        assert(!sceneTabs.empty());
        sceneTabs[activeTab].document = created.previous;
    }

    activeTab = sceneTabs.size();
    addSceneTab(ctx);
    firstVisibleTab = activeTab;
    restoreTabSelection = true;
    Core::Input::resetMouseTracking();
    return created.result;
}

Scribe::Result EditorShell::closeAllSceneTabs(Context &ctx, Engine &engine, Scribe::Session &session)
{
    // Close parked documents directly do not reload each one just to close it.
    for (std::size_t i = sceneTabs.size(); i > 0; --i)
    {
        if (i - 1 == activeTab)
            continue;

        const auto result =
            closeSceneTab(ctx, engine, session, i - 1, activeTab);
        if (!result.ok())
            return result;
    }

    if (sceneTabs.empty())
        return Scribe::Result::noChange();

    assert(sceneTabs.size() == 1 && activeTab == 0);
    return closeSceneTab(ctx, engine, session, 0, 0);
}

bool EditorShell::updateFileMenu(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto *project = engine.getProject();
    const auto projectFile = project ? project->getProjectFilePath() : std::filesystem::path{};

    if (recentProject != projectFile)
    {
        ctx.closeContextMenu();
        recentScenes.clear();
        recentProject = projectFile;

        const auto *scene = engine.getActiveScene();
        if (scene && !engine.isPlaying() && scene->getResourcePath())
            rememberSceneFile(engine, *scene->getResourcePath());
    }

    const bool canEdit = project && engine.hasActiveScene() && !engine.isPlaying() && !session.transformEditActive() && !assetDrag.asset.isValid() && !assetDrag.ownsInput;

    ContextMenuSelection choice;
    if (ctx.pollContextMenuSelection(choice, fileButton))
    {
        if (choice.action == Quit)
        {
            glfwSetWindowShouldClose(
                engine.getWindow()->getWindow(), GLFW_TRUE);
            return true;
        }

        if (!canEdit || fileMenuGeneration != engine.getSceneGeneration())
        {
            contentBrowserUI.reportError(ctx, "The scene changed. Open the File menu again.");
            return true;
        }

        session.endMerge();
        Scribe::Result result = Scribe::Result::noChange();

        switch (choice.action)
        {
        case NewScene:
            result = newSceneTab(ctx, engine, session);
            break;

        case OpenScene:
            openSceneFile(ctx, engine, session);
            return true;

        case SaveScene:
        case SaveSceneAs:
            result = saveScene(
                ctx, engine, session, choice.action == SaveSceneAs);
            break;

        case CloseScene:
            if (!sceneTabs.empty())
                requestSceneTabClose(ctx, engine, session, activeTab);
            return true;

        case CloseAllScenes:
            closingAllScenes = true;
            return true;

        default:
            if (choice.action >= RecentScene &&
                choice.action - RecentScene < recentScenes.size())
            {
                const std::string path =
                    recentScenes[choice.action - RecentScene];
                result = openSceneTab(ctx, engine, session, path);
            }
            break;
        }

        contentBrowserUI.reportError(ctx, result.error);
        return true;
    }

    if (!ctx.isClicked(fileButton))
        return false;

    std::vector<ContextMenuItem> recentItems;
    recentItems.reserve(recentScenes.size());

    for (std::size_t i = 0; i < recentScenes.size(); ++i)
        recentItems.push_back({RecentScene + i, recentScenes[i], true});

    if (recentItems.empty())
        recentItems.push_back({RecentScene, "No recent scenes", false});

    const ContextMenuItem menu[] = {
        {NewScene, "New Scene", canEdit},
        {OpenScene, "Open Scene", canEdit},
        {OpenRecent, "Open Recent",
            canEdit && !recentScenes.empty(), recentItems},
        {SaveScene, "Save Scene", canEdit},
        {SaveSceneAs, "Save Scene As", canEdit},
        {CloseScene, "Close Scene", canEdit},
        {CloseAllScenes, "Close All Scenes", canEdit},
        {Quit, "Quit", true}
    };

    const Rect button = ctx.visibleRect(fileButton);
    if (ctx.openContextMenu(fileButton, {button.x, button.maxY()}, menu))
    {
        fileMenuGeneration = engine.getSceneGeneration();
        session.endMerge();
    }
    else
    {
        contentBrowserUI.reportError(ctx, "Could not open the File menu.");
    }

    return true;
}
} // namespace Cthulhu::Editor