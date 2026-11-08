#include <GLFW/glfw3.h>

#include <string>
#include <algorithm>

#include "editorShell.hpp"
#include "editorUi.hpp"

#include "engine.hpp"
#include "input.hpp"
#include "log_utils.hpp"
#include "scene.hpp"
#include "session.hpp"

using namespace octogui;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
namespace
{
void fillWithChild(Context &ctx, NodeHandle pane)
{
    auto layout = ctx.layoutStyle(pane);
    layout.mode = LayoutMode::Stack;
    layout.horizontalAlignment = Alignment::Stretch;
    layout.verticalAlignment = Alignment::Stretch;
    layout.padding = Padding{};
    layout.gap = 0.0f;
    layout.clip = true;
}

void seeThrough(Context &ctx, NodeHandle node)
{
    ctx.style(node).all.background = Color::transparent();
    ctx.style(node).shadow = ShadowStyle{};
}

void fixedSize(Context &ctx, NodeHandle node, f32 width, f32 height)
{
    auto layout = ctx.layoutStyle(node);
    layout.minSize = Vec2{width, height};
    layout.preferredSize = Vec2{width, height};
}

void verticalSeparator(Context &ctx, NodeHandle parent, f32 height)
{
    const NodeHandle separator = ctx.createSeparator(parent);
    ctx.setSeparatorOrientation(separator, SeparatorOrientation::Vertical);
    fixedSize(ctx, separator, 1.0f, height);
}

IconButton iconButton(Context &ctx, NodeHandle parent, std::string_view icon)
{
    IconButton result;
    result.button = ctx.createButton(parent, "");
    fixedSize(ctx, result.button, 32.0f, 26.0f);

    auto layout = ctx.layoutStyle(result.button);
    layout.mode = LayoutMode::Horizontal;
    layout.padding = Padding{};
    layout.horizontalAlignment = Alignment::Center;
    layout.verticalAlignment = Alignment::Center;

    result.icon = ctx.createIcon(result.button, icon);
    ctx.setIconSize(result.icon, 18.0f);
    return result;
}

void styleToggle(Context &ctx, const IconButton &toggle, bool on)
{
    const Theme &theme = ctx.theme();
    auto style = ctx.style(toggle.button);
    style.normal.background = on ? theme.accent : theme.surface;
    style.normal.border = on ? theme.accentActive : theme.border;
    ctx.setIconColor(toggle.icon, on ? theme.accentText : theme.text);
}

void fixedBar(Context &ctx, NodeHandle bar, f32 height)
{
    auto layout = ctx.layoutStyle(bar);
    layout.mode = LayoutMode::Horizontal;
    layout.verticalAlignment = Alignment::Center;
    layout.gap = 6.0f;
    layout.padding = Padding{8.0f, 2.0f};
    layout.minSize = Vec2{0.0f, height};
    layout.preferredSize = Vec2{0.0f, height};
}
} // namespace

void EditorShell::build(Context &ctx)
{
    const NodeHandle root = ctx.ensureRoot();
    seeThrough(ctx, root);
    auto rootLayout = ctx.layoutStyle(root);
    rootLayout.mode = LayoutMode::Vertical;
    rootLayout.horizontalAlignment = Alignment::Stretch;

    const NodeHandle topBar = ctx.createPanel(root);
    fixedBar(ctx, topBar, 50.0f);
    auto topLayout = ctx.layoutStyle(topBar);
    topLayout.gap = 8.0f;
    topLayout.padding = Padding{12.0f, 2.0f};
    topLayout.horizontalAlignment = Alignment::Start;

    const NodeHandle logo = ctx.createIcon(topBar, "Cthulhu");
    ctx.setIconSize(logo, 46.0f);

    sceneLabel = ctx.createLabel(topBar, "");
    ctx.style(sceneLabel).all.text = ctx.theme().textMuted;
    fixedSize(ctx, sceneLabel, 150.0f, 28.0f);
    verticalSeparator(ctx, topBar, 24.0f);

    saveButton = iconButton(ctx, topBar, "save");
    ctx.layoutStyle(saveButton.button).gap = 6.0f;
    [[maybe_unused]] const NodeHandle saveLabel = ctx.createLabel(saveButton.button, "Save Scene");
    fixedSize(ctx, saveButton.button, 120.0f, 26.0f);
    verticalSeparator(ctx, topBar, 24.0f);

    [[maybe_unused]] const NodeHandle controlsSpacer = ctx.createContainer(topBar);

    playButton = iconButton(ctx, topBar, "play");
    pauseButton = iconButton(ctx, topBar, "pause");
    stopButton = iconButton(ctx, topBar, "stop");
    styleToggle(ctx, playButton, false);
    styleToggle(ctx, pauseButton, false);
    ctx.setEnabled(pauseButton.button, false);
    ctx.setEnabled(stopButton.button, false);
    ctx.setIconColor(pauseButton.icon, ctx.theme().textDisabled);
    ctx.setIconColor(stopButton.icon, ctx.theme().textDisabled);

    // left >> hierarchy viewport and browser. right >> inspector.
    const NodeHandle workspace = ctx.createSplitContainer(root, SplitOrientation::Horizontal, 0.80f);
    ctx.setSplitMinimumSizes(workspace, 444.0f, 280.0f);
    seeThrough(ctx, workspace);

    const NodeHandle mainPane = ctx.splitFirstPane(workspace);
    fillWithChild(ctx, mainPane);
    seeThrough(ctx, mainPane);

    // top >> hierarchy and viewport. bottom >> content browser
    const NodeHandle mainContent = ctx.createSplitContainer(mainPane, SplitOrientation::Vertical, 0.72f);
    ctx.setSplitMinimumSizes(mainContent, 260.0f, 180.0f);
    seeThrough(ctx, mainContent);

     const NodeHandle upperPane = ctx.splitFirstPane(mainContent);
    fillWithChild(ctx, upperPane);
    seeThrough(ctx, upperPane);

    const NodeHandle hierarchyViewport = ctx.createSplitContainer(upperPane, SplitOrientation::Horizontal, 0.18f);
    ctx.setSplitMinimumSizes(hierarchyViewport, 180.0f, 260.0f);
    seeThrough(ctx, hierarchyViewport);

    hierarchyPanelUI.build(ctx, ctx.splitFirstPane(hierarchyViewport));

    sceneNode = ctx.splitSecondPane(hierarchyViewport);
    fillWithChild(ctx, sceneNode);
    seeThrough(ctx, sceneNode);
    viewportPanelUI.build(ctx, sceneNode);

    const NodeHandle inspectorPane = ctx.splitSecondPane(workspace);
    fillWithChild(ctx, inspectorPane);
    const NodeHandle inspector = createTitledPanel(ctx, inspectorPane, "sliders", "INSPECTOR");
    inspectorUI.build(ctx, inspector);

    contentBrowserUI.build(ctx, ctx.splitSecondPane(mainContent));

    const NodeHandle statusBar = ctx.createPanel(root);
    fixedBar(ctx, statusBar, 24.0f);
}

void EditorShell::frame(EditorUi &ui, Engine &engine, Scribe::Session &session, void (*routeInput)(void *, float))
{
    engine.update();

    if (!engine.getWindow())
        return;

    ui.beginFrame(engine.getDeltaTime());

    if (routeInput)
    {
        routeInput(&engine, engine.getDeltaTime());
    }

    update(ui.context(), engine, session);
    ui.context().resolvePendingLayout();
    engine.refreshSceneTransforms();

    const PixelRect viewport = ui.pixelRect(sceneNode);
    engine.setViewportRect(
        viewport.x, viewport.y, viewport.width, viewport.height);

    engine.render(viewportPanelUI.overlayVertices());
    ui.endFrame();
    engine.present();
}

void EditorShell::update(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const std::uint64_t inputGeneration = engine.getSceneGeneration();

    const auto changes = session.takeEvents();
    const bool assetsChanged = std::any_of(
        changes.begin(), changes.end(),
        [](const Scribe::ChangeEvent &change) {
            switch (change.type)
            {
                case Scribe::ChangeType::AssetImported:
                case Scribe::ChangeType::AssetMoved:
                case Scribe::ChangeType::AssetDeleted:
                case Scribe::ChangeType::AssetSettingsChanged:
                case Scribe::ChangeType::AssetRegistryRefreshed:
                    return true;
                default:
                    return false;
            }
        });
    contentBrowserUI.update(ctx, engine, session, assetsChanged);
    hierarchyPanelUI.update(ctx, engine, session, changes);

    uiEvents.clear();
    octogui::UIEvent event;
    while (ctx.pollEvent(event))
    {
        uiEvents.push_back(event);
    }

    viewportPanelUI.update(ctx);

    const bool wasPlaying = engine.isPlaying();
    const bool togglePlay = !ctx.contextMenuOwnsInput() && !ctx.isValid(ctx.focused()) && Core::Input::isKeyPressed(GLFW_KEY_F5);

    if (ctx.isClicked(saveButton.button))
    {
        const auto result = session.save();
        if (!result.ok())
        {
            Log::Print("SAVE FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
        }
    }

    if ((ctx.isClicked(playButton.button) || togglePlay) && !wasPlaying)
    {
        if (!viewportPanelUI.finishGizmoDrag(session, false))
        {
            return;
        }

        const bool ok = engine.play();
        (void)ok;
    }

    if (ctx.isClicked(pauseButton.button) && engine.isPlaying())
    {
        const bool paused = engine.getSimulationState() == SimulationState::Paused;
        engine.setSimulationState(paused ? SimulationState::Running : SimulationState::Paused);
    }

    if ((ctx.isClicked(stopButton.button) || togglePlay) && wasPlaying)
    {
        const bool ok = engine.stop();
        (void)ok;
    }

    const bool playing = engine.isPlaying();
    const bool paused = playing && engine.getSimulationState() != SimulationState::Running;

    if (paused != lastPaused)
    {
        styleToggle(ctx, pauseButton, paused);
        lastPaused = paused;
    }

    if (playing != lastPlaying)
    {
        ctx.setEnabled(saveButton.button, !playing);
        ctx.setEnabled(playButton.button, !playing);
        ctx.setEnabled(pauseButton.button, playing);
        ctx.setEnabled(stopButton.button, playing);

        ctx.setIconColor(playButton.icon, playing ? ctx.theme().textDisabled : ctx.theme().text);
        ctx.setIconColor(pauseButton.icon, playing ? ctx.theme().text : ctx.theme().textDisabled);
        ctx.setIconColor(stopButton.icon, playing ? ctx.theme().text : ctx.theme().textDisabled);

        lastPlaying = playing;
    }

    if (ctx.input().mouse.pressed(MouseButton::Left))
    {
        viewportPanelUI.updateGizmo(ctx, engine, session, hierarchyPanelUI, sceneNode);
    }

    viewportPanelUI.updateMarquee(ctx,
                                  sceneNode,
                                  !engine.isPlaying() && engine.hasActiveScene(),
                                  engine.getSceneGeneration());
    viewportPanelUI.updatePicking(ctx, engine, hierarchyPanelUI, sceneNode);
    if (engine.isPlaying())
        engine.getRenderer().setOutlineTargets({});
    else
        engine.getRenderer().setOutlineTargets(hierarchyPanelUI.selection());
    inspectorUI.update(ctx, engine, session, engine.getActiveScene(), 
                        hierarchyPanelUI.selectedEntity(),
                        uiEvents, inputGeneration, assetsChanged);
    viewportPanelUI.updateGizmo(ctx, engine, session, hierarchyPanelUI, sceneNode);
    viewportPanelUI.drawGizmoFeedback(ctx);

    std::string sceneText = "No scene";
    if (const auto *activeScene = engine.getActiveScene())
    {
        sceneText = activeScene->getName() + (activeScene->isDirty() ? " *" : "");
    }

    if (sceneText != lastSceneText)
    {
        ctx.setText(sceneLabel, sceneText);
        lastSceneText = sceneText;
    }
}
} // namespace Cthulhu::Editor
