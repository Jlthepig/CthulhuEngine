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

ContextMenuItem commandItem(Action action, std::string_view text, bool enabled, bool checked = false)
{
    return {
        static_cast<u64>(action), text, enabled, {},
        Shortcuts::label(action), false, checked
    };
}
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

    if (Shortcuts::isLayoutAction(action))
    {
        if (session.transformEditActive() ||
            assetDrag.asset.isValid() ||
            assetDrag.ownsInput ||
            contentBrowserUI.actionPending() ||
            exitDialog.isOpen(ctx) ||
            restoreTabSelection)
            return false;

        session.endMerge();
        return changeLayout(ctx, action);
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
    
    case Action::Copy:
    case Action::Cut:
        result = session.copyEntities(hierarchyPanelUI.selection(), action == Action::Cut);

        if (action == Action::Cut && result.status == Scribe::Result::Status::Applied)
        {
            hierarchyPanelUI.selectEntity(ctx, std::nullopt);
        }
        break;

    case Action::Delete:
        result = session.deleteEntities(hierarchyPanelUI.selection());

        if (result.status == Scribe::Result::Status::Applied)
            hierarchyPanelUI.selectEntity(ctx, std::nullopt);
        break;

    case Action::Duplicate:
    case Action::Paste:
    {
        auto created = action == Action::Duplicate ? session.duplicateEntities(hierarchyPanelUI.selection()) : session.pasteEntities();

        result = std::move(created.result);

        if (result.status == Scribe::Result::Status::Applied)
        {
            pendingEntitySelection = std::move(created.ids);
            pendingEntitySelectionGeneration = engine.getSceneGeneration();
        }
        break;
    }

    case Action::Rename:
        if (!layoutSettings.hierarchy && hierarchyPanelUI.selectedEntity())
            (void)changeLayout(ctx, Action::ToggleHierarchy);
        hierarchyPanelUI.requestRename(ctx);
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

    const ContextMenuItem separator{0, {}, false, {}, {}, true};

    const ContextMenuItem menu[] = {
        commandItem(Action::NewScene, "New Scene", canEdit),
        commandItem(Action::OpenScene, "Open Scene", canEdit),
        {OpenRecent, "Open Recent",
            canEdit && !recentScenes.empty(), recentItems},
        separator,
        commandItem(Action::SaveScene, "Save Scene", canEdit),
        commandItem(Action::SaveSceneAs, "Save Scene As", canEdit),
        separator,
        commandItem(Action::CloseScene, "Close Scene", canEdit),
        commandItem(Action::CloseAllScenes, "Close All Scenes", canEdit),
        separator,
        commandItem(Action::Quit, "Quit", true)
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

bool EditorShell::updateEditViewMenus(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const bool idle =
        !session.transformEditActive() &&
        !assetDrag.asset.isValid() &&
        !assetDrag.ownsInput &&
        !contentBrowserUI.actionPending() &&
        !exitDialog.isOpen(ctx) &&
        !restoreTabSelection;

    const bool canEdit = idle && engine.hasActiveScene() && !engine.isPlaying();

    ContextMenuSelection choice;
    if (ctx.pollContextMenuSelection(choice, editButton))
    {
        if (!canEdit || fileMenuGeneration != engine.getSceneGeneration())
        {
            contentBrowserUI.reportError(ctx, "The scene changed. Open the Edit menu again.");
        }
        else
        {
            (void)executeCommand(ctx, engine, session, static_cast<Action>(choice.action));
        }
        return true;
    }

    if (ctx.pollContextMenuSelection(choice, viewButton))
    {
        if (idle)
        {
            (void)executeCommand(ctx, engine, session, static_cast<Action>(choice.action));
        }
        return true;
    }

    const bool edit = ctx.isClicked(editButton);
    const bool view = ctx.isClicked(viewButton);
    if ((!edit && !view) || !idle) return false;

    const ContextMenuItem separator{0, {}, false, {}, {}, true};
    const bool selected = canEdit && !hierarchyPanelUI.selection().empty();

    const ContextMenuItem editItems[] = {
        commandItem(Action::Undo, "Undo", canEdit && session.canUndo()),
        commandItem(Action::Redo, "Redo", canEdit && session.canRedo()),
        separator,
        commandItem(Action::Cut, "Cut", selected),
        commandItem(Action::Copy, "Copy", selected),
        commandItem(
            Action::Paste, "Paste", canEdit && session.canPasteEntities())
    };

    const ContextMenuItem layoutItems[] = {
        commandItem(Action::ResetLayout, "Reset to Default", true)
    };

    const ContextMenuItem viewItems[] = {
        {1000, "Appearance (unavailable)", false},
        {1001, "Editor Layout", true, layoutItems},
        separator,
        commandItem(
            Action::ToggleHierarchy, "Hierarchy",
            true, layoutSettings.hierarchy),
        commandItem(
            Action::ToggleInspector, "Inspector",
            true, layoutSettings.inspector),
        commandItem(
            Action::ToggleContentBrowser, "Content Browser",
            true, layoutSettings.browser)
    };

    const auto owner = edit ? editButton : viewButton;
    const Rect button = ctx.visibleRect(owner);
    const auto items = edit ? std::span<const ContextMenuItem>{editItems} : std::span<const ContextMenuItem>{viewItems};

    if (ctx.openContextMenu(owner, {button.x, button.maxY()}, items))
    {
        fileMenuGeneration = engine.getSceneGeneration();
        session.endMerge();
    }
    else
    {
        contentBrowserUI.reportError(ctx, "Could not open the editor menu.");
    }

    return true;
}
} // namespace Cthulhu::Editor