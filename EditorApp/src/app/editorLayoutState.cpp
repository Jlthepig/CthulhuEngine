#include "editorShell.hpp"

#include <cassert>

namespace Cthulhu::Editor
{
void EditorShell::captureLayout(Context &ctx)
{
    for (std::size_t i = 0; i < layoutSplits.size(); ++i)
        layoutSettings.ratios[i] = ctx.splitRatio(layoutSplits[i]);
}

void EditorShell::applyLayout(Context &ctx)
{
    for (std::size_t i = 0; i < layoutSplits.size(); ++i)
    {
        assert(ctx.isValid(layoutSplits[i]));
        ctx.setSplitRatio(layoutSplits[i], layoutSettings.ratios[i]);
    }

    ctx.setVisible(ctx.splitFirstPane(layoutSplits[2]), layoutSettings.hierarchy);
    ctx.setVisible(ctx.splitSecondPane(layoutSplits[0]), layoutSettings.inspector);
    ctx.setVisible(ctx.splitSecondPane(layoutSplits[1]), layoutSettings.browser);

    if (ctx.isValid(ctx.focused()) &&
        !ctx.tree().isVisibleBranch(ctx.focused()))
        ctx.clearFocus();
}

void EditorShell::saveLayout(Context &ctx, bool force)
{
    captureLayout(ctx);

    if (force || layoutSavePending || layoutSettings != lastLayoutAttempt)
    {
        saveEditorLayout(layoutSettings);
        lastLayoutAttempt = layoutSettings;
        layoutSavePending = false;
    }
}

bool EditorShell::changeLayout(Context &ctx, Shortcuts::Action action)
{
    if (!Shortcuts::isLayoutAction(action)) return false;
    captureLayout(ctx);

    switch (action)
    {
    case Shortcuts::Action::ResetLayout:
        layoutSettings = EditorLayout{};
        break;

    case Shortcuts::Action::ToggleHierarchy:
        layoutSettings.hierarchy = !layoutSettings.hierarchy;
        break;

    case Shortcuts::Action::ToggleInspector:
        layoutSettings.inspector = !layoutSettings.inspector;
        break;

    case Shortcuts::Action::ToggleContentBrowser:
        layoutSettings.browser = !layoutSettings.browser;
        break;

    default:
        return false;
    }

    applyLayout(ctx);
    layoutSavePending = true;
    return true;
}
}