#include "editorShell.hpp"

#include <GLFW/glfw3.h>

#include "engine.hpp"
#include "input.hpp"
#include "session.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
bool EditorShell::handleExit(Context &ctx, Engine &engine, Scribe::Session &session)
{
    if (exitDialog.isOpen(ctx))
    {
        const auto choice = exitDialog.pollChoice(ctx);
        ctx.clearEvents();
        switch (choice)
        {
            case DialogChoice::None:
                return false;
            case DialogChoice::Cancel:
                exitDialog.hide(ctx);
                if (resumePlayOnCancel && engine.isPlaying())
                    engine.setSimulationState(SimulationState::Running);
                Core::Input::resetMouseTracking();
                return false;
            case DialogChoice::Secondary:
                return true;
            case DialogChoice::Primary:
                if (engine.isPlaying() && !engine.stop())
                {
                    exitDialog.setError(ctx, "Could not return to the edit scene.");
                    return false;
                }

                if (const auto saved = session.save(); !saved.ok())
                {
                    exitDialog.setError(ctx, saved.error);
                    return false;
                }
                return true;
        }
    }

    ctx.closeContextMenu();
    ctx.clearFocus();
    clearAssetDrag(engine);
    assetFeedback.clear();
    contentBrowserUI.clearFolderDropHighlight(ctx);

    const auto generation = engine.getSceneGeneration();
    viewportPanelUI.updateMarquee(ctx, sceneNode, false, generation);
    if (!viewportPanelUI.finishGizmoDrag(session, false))
    {
        contentBrowserUI.reportError(ctx, "Could not finish the transform preview. Try closing again.");
        ctx.clearEvents();
        return false;
    }

    uiEvents.clear();
    UIEvent event;
    while (ctx.pollEvent(event))
    {
        if (event.type != UIEventType::ValueChanged || !ctx.isValid(event.target))
            continue;

        const auto type = ctx.tree().type(event.target);
        if (type == NodeType::TextInput || type == NodeType::NumericInput)
            uiEvents.push_back(event);
    }

    inspectorUI.update(ctx, engine, session, engine.getActiveScene(), hierarchyPanelUI.selectedEntity(),
                       uiEvents, generation, false);
    session.endMerge();

    const auto name = engine.unsavedEditSceneName();
    if (!name)
        return true;

    resumePlayOnCancel = engine.isPlaying() && engine.getSimulationState() == SimulationState::Running;
    if (resumePlayOnCancel)
        engine.setSimulationState(SimulationState::Paused);

    glfwSetInputMode(engine.getWindow()->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    Core::Input::resetMouseTracking();
    exitDialog.show(ctx, "Save changes before exiting?", name->empty() ? "Untitled" : *name, "Save", "Discard Changes");
    return false;
}
} // namespace Cthulhu::Editor