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
using Action = Shortcuts::Action;

constexpr u64 OpenRecent = 100;
constexpr u64 RecentScene = 101;

static_assert(static_cast<u64>(Action::Count) < OpenRecent);
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

bool EditorShell::executeCommand(Context &ctx, Engine &engine, Scribe::Session &session, Shortcuts::Action action)
{
    using Action = Shortcuts::Action;

    if (action == Action::Quit)
    {
        glfwSetWindowShouldClose(engine.getWindow()->getWindow(), GLFW_TRUE);
        return true;
    }

    if (!engine.hasActiveScene() || engine.isPlaying() ||
        session.transformEditActive() ||
        assetDrag.asset.isValid() || assetDrag.ownsInput)
    {
        return false;
    }

    session.endMerge();
    Scribe::Result result = Scribe::Result::noChange();

    switch (action)
    {
    case Action::Undo:
        if (session.canUndo() && !session.undo())
            result = Scribe::Result::failed("Could not undo the last edit. See the log.");
        break;

    case Action::Redo:
        if (session.canRedo() && !session.redo())
            result = Scribe::Result::failed("Could not redo the last edit. See the log.");
        break;

    case Action::NewScene:
        result = newSceneTab(ctx, engine, session);
        break;

    case Action::OpenScene:
        openSceneFile(ctx, engine, session);
        return true;

    case Action::SaveScene:
    case Action::SaveSceneAs:
        result = saveScene(ctx, engine, session, action == Action::SaveSceneAs);
        break;

    case Action::CloseScene:
        if (!sceneTabs.empty())
            requestSceneTabClose(ctx, engine, session, activeTab);
        return true;

    case Action::CloseAllScenes:
        closingAllScenes = true;
        return true;

    default:
        return false;
    }

    contentBrowserUI.reportError(ctx, result.error);
    return true;
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
        if (choice.action == static_cast<u64>(Action::Quit))
        {
            (void)executeCommand(ctx, engine, session, Action::Quit);
            return true;
        }

        if (!canEdit || fileMenuGeneration != engine.getSceneGeneration())
        {
            contentBrowserUI.reportError(ctx, "The scene changed. Open the File menu again.");
            return true;
        }

        if (choice.action >= RecentScene && choice.action - RecentScene < recentScenes.size())
        {
            session.endMerge();

            const std::string path = recentScenes[choice.action - RecentScene];
            const auto result = openSceneTab(ctx, engine, session, path);
            contentBrowserUI.reportError(ctx, result.error);
        }
        else
        {
            (void)executeCommand(ctx, engine, session, static_cast<Action>(choice.action));
        }
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
        {static_cast<u64>(Action::NewScene), "New Scene", canEdit},
        {static_cast<u64>(Action::OpenScene), "Open Scene", canEdit},
        {OpenRecent, "Open Recent",
            canEdit && !recentScenes.empty(), recentItems},
        {static_cast<u64>(Action::SaveScene), "Save Scene", canEdit},
        {static_cast<u64>(Action::SaveSceneAs), "Save Scene As", canEdit},
        {static_cast<u64>(Action::CloseScene), "Close Scene", canEdit},
        {static_cast<u64>(Action::CloseAllScenes), "Close All Scenes", canEdit},
        {static_cast<u64>(Action::Quit), "Quit", true}
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