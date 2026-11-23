#include <GLFW/glfw3.h>

#include <string>
#include <algorithm>
#include <iterator>

#include "editorShell.hpp"
#include "editorUi.hpp"

#include "engine.hpp"
#include "input.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "window.hpp"

using namespace octogui;

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
    fillWithChild(ctx, root);

    const NodeHandle editorContent = ctx.createContainer(root);
    auto editorLayout = ctx.layoutStyle(editorContent);
    editorLayout.mode = LayoutMode::Vertical;
    editorLayout.horizontalAlignment = Alignment::Stretch;

    const NodeHandle topBar = ctx.createPanel(editorContent);
    fixedBar(ctx, topBar, 50.0f);
    auto topLayout = ctx.layoutStyle(topBar);
    topLayout.gap = 8.0f;
    topLayout.padding = Padding{12.0f, 2.0f};
    topLayout.horizontalAlignment = Alignment::Start;

    const NodeHandle logo = ctx.createIcon(topBar, "Cthulhu");
    ctx.setIconSize(logo, 46.0f);

    verticalSeparator(ctx, topBar, 24.0f);

    const char *menuNames[] = {"File", "Edit", "View", "Project"};
    NodeHandle *menuButtons[] = {&fileButton, &editButton, &viewButton, &projectButton};

    for (std::size_t i = 0; i < std::size(menuNames); ++i)
    {
        const auto button = ctx.createButton(topBar, menuNames[i]);
        *menuButtons[i] = button;
        fixedSize(ctx, button, 52.0f, 28.0f);

        auto style = ctx.style(button);
        style.all.background = Color::transparent();
        style.all.gradient = LinearGradient{};
        style.borderWidth = 0.0f;
        style.pressedOffsetY = 0.0f;
        style.shadow = ShadowStyle{};
        style.hovered.background = ctx.theme().surfaceHovered;
        style.active.background = ctx.theme().surfaceActive;
    }

    fixedSize(ctx, projectButton, 72.0f, 28.0f);

    (void)ctx.createContainer(topBar);

    playButton = iconButton(ctx, topBar, "play");
    playCurrentSceneButton = iconButton(ctx, topBar, "play-current-scene");
    pauseButton = iconButton(ctx, topBar, "pause");
    stopButton = iconButton(ctx, topBar, "stop");
    styleToggle(ctx, playButton, false);
    styleToggle(ctx, playCurrentSceneButton, false);

    const auto setPlayTooltip = [&](NodeHandle button, std::string text, Shortcuts::Action action) {
        const auto shortcut = Shortcuts::label(action);
        if (!shortcut.empty())
            text.append(" (").append(shortcut).append(")");
        ctx.setTooltip(button, text);
    };
    setPlayTooltip(playButton.button, "Play Main Scene", Shortcuts::Action::TogglePlay);
    setPlayTooltip(playCurrentSceneButton.button, "Play Current Scene", Shortcuts::Action::PlayCurrentScene);
    setPlayTooltip(stopButton.button, "Stop", Shortcuts::Action::TogglePlay);

    styleToggle(ctx, pauseButton, false);
    ctx.setEnabled(pauseButton.button, false);
    ctx.setEnabled(stopButton.button, false);
    ctx.setIconColor(pauseButton.icon, ctx.theme().textDisabled);
    ctx.setIconColor(stopButton.icon, ctx.theme().textDisabled);

    // left >> hierarchy viewport and browser. right >> inspector.
    const NodeHandle workspace = ctx.createSplitContainer(editorContent, SplitOrientation::Horizontal, 0.80f);
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

    const auto viewportPane = ctx.splitSecondPane(hierarchyViewport);
    ctx.layoutStyle(viewportPane).mode = LayoutMode::Vertical;
    ctx.layoutStyle(viewportPane).gap = 0.0f;
    ctx.layoutStyle(viewportPane).clip = true;
    seeThrough(ctx, viewportPane);

    buildSceneTabBar(ctx, viewportPane);
    sceneNode = ctx.createContainer(viewportPane);
    fillWithChild(ctx, sceneNode);
    seeThrough(ctx, sceneNode);
    viewportPanelUI.build(ctx, sceneNode);

    const NodeHandle inspectorPane = ctx.splitSecondPane(workspace);
    fillWithChild(ctx, inspectorPane);
    const NodeHandle inspector = createTitledPanel(ctx, inspectorPane, "sliders", "INSPECTOR");
    inspectorUI.build(ctx, inspector);

    contentBrowserUI.build(ctx, ctx.splitSecondPane(mainContent));

    const NodeHandle statusBar = ctx.createPanel(editorContent);
    fixedBar(ctx, statusBar, 24.0f);
    missingAssetsButton = ctx.createButton(statusBar, "Missing Assets...");
    fixedSize(ctx, missingAssetsButton, 140.0f, 22.0f);
    cameraStatusLabel = ctx.createLabel(statusBar, "");
    ctx.setVisible(cameraStatusLabel, false);
    exitDialog.build(ctx, root, editorContent);

    layoutSplits = {
        workspace, mainContent, hierarchyViewport,
        contentBrowserUI.layoutSplit()
    };
    layoutSettings = loadEditorLayout();
    lastLayoutAttempt = layoutSettings;
    applyLayout(ctx);
}

void EditorShell::setPlayInputCaptured(Engine &engine, bool captured)
{
   const auto *window = engine.getWindow();
   if (!window || !window->getWindow())
   {
       playInputCaptured = false;
       capturedSceneGeneration = 0;
       return;
   }

   GLFWwindow *nativeWindow = window->getWindow();
   const int wanted = captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;

   if (playInputCaptured == captured && glfwGetInputMode(nativeWindow, GLFW_CURSOR) == wanted)
   {
       return;
   }

   glfwSetInputMode(nativeWindow, GLFW_CURSOR, wanted);
   playInputCaptured = captured && glfwGetInputMode(nativeWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;

   capturedSceneGeneration = playInputCaptured ? engine.getSceneGeneration() : 0;

   Core::Input::resetMouseTracking();
   (void)Core::Input::getScrollDeltaY();

   if (playInputCaptured)
   {
       Core::Input::suppressMouseButtonUntilRelease(GLFW_MOUSE_BUTTON_LEFT);
   }
}

void EditorShell::updatePlayControls(Context &ctx, Engine &engine)
{
    const bool playing = engine.isPlaying();
    const auto simulation = engine.getSimulationState();
    const bool paused =
        playing && simulation != SimulationState::Running;
    const bool playingChanged = playing != lastPlaying;

    if (playingChanged)
    {
        ctx.setEnabled(pauseButton.button, playing);
        ctx.setEnabled(stopButton.button, playing);

        const auto playColor = playing ? ctx.theme().textDisabled : ctx.theme().text;
        ctx.setIconColor(playButton.icon, playColor);
        ctx.setIconColor(playCurrentSceneButton.icon, playColor);

        ctx.setIconColor(
            stopButton.icon,
            playing ? ctx.theme().text : ctx.theme().textDisabled);
    }

    if (paused != lastPaused || playingChanged)
    {
        styleToggle(ctx, pauseButton, paused);

        if (!playing)
            ctx.setIconColor(pauseButton.icon, ctx.theme().textDisabled);

        ctx.setTooltip(
            pauseButton.button,
            paused ? "Resume simulation" : "Pause simulation");
    }

    const bool canPlay =
        !playing && engine.hasActiveScene() && !restoreTabSelection &&
        !assetDrag.asset.isValid() && !assetDrag.ownsInput;

    ctx.setEnabled(playButton.button, canPlay);
    ctx.setEnabled(playCurrentSceneButton.button, canPlay);

    lastPlaying = playing;
    lastPaused = paused;
}

void EditorShell::startScenePlay(Context &ctx, Engine &engine, Scribe::Session &session, bool currentScene)
{
    if (currentScene)
    {
        if (!engine.play())
        {
            playDialogOpen = true;
            exitDialog.show(ctx, "Could not start Play Current Scene", "The edit scene is still active. See the log.",
                            "", "", "OK");
            return;
        }
    }
    else if (const auto launch = session.playMainScene(); !launch.result.ok())
    {
        playDialogOpen = true;
        if (launch.needsMainScene)
            exitDialog.show(ctx, "Choose a main scene", launch.result.error, "Choose Scene...", "");
        else
            exitDialog.show(ctx, "Could not start Play", launch.result.error, "", "", "OK");
        return;
    }

    ctx.clearEvents();
    Core::Input::resetMouseTracking();
    (void)Core::Input::getScrollDeltaY();
}

void EditorShell::handlePlayDialog(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto choice = exitDialog.pollChoice(ctx);
    ctx.clearEvents();
    if (choice == DialogChoice::Cancel)
    {
        playDialogOpen = false;
        exitDialog.hide(ctx);
        return;
    }
    if (choice != DialogChoice::Primary)
        return;

    const auto picked = pickProjectMainScene(ctx, *engine.getWindow(), engine, session);
    if (!picked.ok())
    {
        exitDialog.setError(ctx, picked.error);
        return;
    }
    if (picked.status == Scribe::Result::Status::NoChange)
        return;

    playDialogOpen = false;
    exitDialog.hide(ctx);
    startScenePlay(ctx, engine, session, false);
}

bool EditorShell::frame(EditorUi &ui, Engine &engine, Scribe::Session &session, void (*routeInput)(void *, float))
{
   if (!engine.prepareFrame())
       return false;

    const auto *window = engine.getWindow();
    if (!window || !window->getWindow())
        return false;

    const bool quitRequested = engine.shouldClose();
    if (quitRequested)
        glfwSetWindowShouldClose(window->getWindow(), GLFW_FALSE);
    const bool closeRequested = quitRequested && !projectSettingsWindow.deferQuit();
    const bool waitingForInput = projectSettingsWindow.waitingForQuit();

    auto &ctx = ui.context();
    GLFWwindow *nativeWindow = window->getWindow();

    const bool beganPlaying = engine.isPlaying();
    const bool beganCaptured = playInputCaptured;
    const bool windowFocused = glfwGetWindowAttrib(nativeWindow, GLFW_FOCUSED) == GLFW_TRUE &&
                               glfwGetWindowAttrib(nativeWindow, GLFW_ICONIFIED) == GLFW_FALSE;

    const bool blocked = closeRequested || waitingForInput || closingAllScenes || exitDialog.isOpen(ctx) ||
                         ctx.contextMenuOwnsInput() || !windowFocused;

    const bool mustRelease = blocked || !engine.isPlaying() || !engine.hasActiveScene() || engine.getSimulationState() != SimulationState::Running ||
        capturedSceneGeneration != engine.getSceneGeneration() || Core::Input::isKeyPressed(GLFW_KEY_ESCAPE) ||
        glfwGetInputMode(nativeWindow, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

    if ((playInputCaptured && mustRelease) || (!playInputCaptured && (beganPlaying || blocked)))
    {
        setPlayInputCaptured(engine, false);
    }

    const auto shortcut = ui.beginFrame(engine.getDeltaTime(), beganCaptured || !windowFocused);

    if (!ctx.input().mouse.wasDown(MouseButton::Left))
        assetDrag.ownsInput = false;

    if (closeRequested && referenceRepairOpen)
    {
        referenceRepairOpen = false;
        deletingAsset = {};
        exitDialog.hide(ctx);
    }

    if (closeRequested && closingAllScenes)
    {
        closingAllScenes = false;
        exitDialog.hide(ctx);
    }

    if (closeRequested && deletingAsset.isValid())
    {
        exitDialog.hide(ctx);
        deletingAsset = {};
    }

    if (closeRequested && playDialogOpen)
    {
        playDialogOpen = false;
        exitDialog.hide(ctx);
    }

    const bool handlingExit = closeRequested || waitingForInput || closingAllScenes || exitDialog.isOpen(ctx);
    if (handlingExit && !waitingForInput)
    {
        if (playDialogOpen)
            handlePlayDialog(ctx, engine, session);
        else if (referenceRepairOpen)
            handleReferenceRepair(ctx, engine, session);
        else if (deletingAsset.isValid())
            handleAssetDelete(ctx, engine, session);
        else if (handleExit(ctx, engine, session))
        {
            ctx.endFrame();
            return false;
        }
    }
    else if (!handlingExit)
    {
        if (routeInput && !beganCaptured && windowFocused && !assetDrag.asset.isValid() && !assetDrag.ownsInput)
        {
            routeInput(&engine, engine.getDeltaTime());
        }

       update(ctx, engine, session, windowFocused ? shortcut : Shortcuts::Action::None);
    }

    const bool canCapture =
    engine.isPlaying() && engine.hasActiveScene() &&
    engine.getSimulationState() == SimulationState::Running &&
    windowFocused && !handlingExit && !closingAllScenes &&
    !exitDialog.isOpen(ctx) && !ctx.contextMenuOwnsInput() &&
    !engine.shouldClose() &&
    !assetDrag.asset.isValid() && !assetDrag.ownsInput;

    if (!canCapture || !beganPlaying)
    {
        if (playInputCaptured || beganPlaying || engine.isPlaying() || handlingExit || exitDialog.isOpen(ctx) || !windowFocused)
        {
            setPlayInputCaptured(engine, false);
        }
    }
    else if (!playInputCaptured && !beganCaptured && !ui.wantsMouse() && ctx.input().mouse.pressed(MouseButton::Left) && !Core::Input::isKeyDown(GLFW_KEY_ESCAPE))
    {
        ctx.clearFocus();
        ctx.clearEvents();
        setPlayInputCaptured(engine, true);
    }

    engine.advanceSimulation(canCapture && playInputCaptured);

    const auto *currentWindow = engine.getWindow();
    if (!currentWindow || !currentWindow->getWindow())
    {
        playInputCaptured = false;
        ctx.endFrame();
        return false;
    }

    if (playInputCaptured && (!engine.isPlaying() || !engine.hasActiveScene() || engine.getSimulationState() != SimulationState::Running ||
        capturedSceneGeneration != engine.getSceneGeneration() || engine.shouldClose() ||
        glfwGetWindowAttrib(currentWindow->getWindow(), GLFW_FOCUSED) != GLFW_TRUE))
    {
        setPlayInputCaptured(engine, false);
    }

    updatePlayControls(ctx, engine);

    ctx.resolvePendingLayout();
    engine.refreshSceneTransforms();
    engine.getRenderer().setGridVisible(engine.getWorldMode() == WorldMode::Edit);

    const PixelRect viewport = ui.pixelRect(sceneNode);
    engine.setViewportRect(viewport.x, viewport.y, viewport.width, viewport.height);
    Rendering::ModelPreview preview;
    if (assetPreviewPosition)
        preview = {engine.getAssetManager().getModel(assetDrag.previewModel), *assetPreviewPosition};

    const auto overlay = handlingExit || exitDialog.isOpen(ctx) || !engine.hasActiveScene() || engine.isPlaying() || restoreTabSelection || 
                        assetDrag.ownsInput ? std::span<const Rendering::OverlayVertex>{} : viewportPanelUI.overlayVertices();
    engine.render(overlay, preview);

    const auto cameraStatus = engine.getGameCameraStatus();
    if (cameraStatus != displayedCameraStatus)
    {
        const auto message = Scene::gameCameraMessage(cameraStatus);
        ctx.setText(cameraStatusLabel, message);
        ctx.setVisible(cameraStatusLabel, !message.empty());
        ctx.resolvePendingLayout();
        displayedCameraStatus = cameraStatus;
    }

    const auto &mouse = ctx.input().mouse;
    bool mouseBusy = false;

    for (const auto button : {
        MouseButton::Left, MouseButton::Right, MouseButton::Middle})
    {
        mouseBusy |= mouse.isDown(button) || mouse.wasDown(button);
    }

    if (!mouseBusy)
        saveLayout(ctx);

    ui.endFrame(assetFeedback.span());
    projectSettingsWindow.frame(engine, session);
    engine.present();
    return true;
}

void EditorShell::update(Context &ctx, Engine &engine, Scribe::Session &session, Shortcuts::Action shortcut)
{
    assetFeedback.clear();
    assetPreviewPosition.reset();
    contentBrowserUI.clearFolderDropHighlight(ctx);

    if (!engine.hasActiveScene() && !engine.createEmptyScene("Untitled"))
    {
        contentBrowserUI.reportError(ctx, "Could not create the empty scene.");
        return;
    }

    if (sceneTabs.empty() && engine.hasActiveScene())
        addSceneTab(ctx);

    const std::uint64_t inputGeneration = engine.getSceneGeneration();

    if (assetDrag.asset.isValid() || assetDrag.ownsInput || session.transformEditActive() || restoreTabSelection || contentBrowserUI.actionPending() 
        || (engine.isPlaying() && shortcut != Shortcuts::Action::TogglePlay && !Shortcuts::isLayoutAction(shortcut)))
    {
        shortcut = Shortcuts::Action::None;
    }

    if (shortcut == Shortcuts::Action::SaveScene || shortcut == Shortcuts::Action::SaveSceneAs)
    {
        ctx.clearFocus();
    }

    switch (shortcut)
    {
    case Shortcuts::Action::Cut:
    case Shortcuts::Action::Copy:
    case Shortcuts::Action::Paste:
    case Shortcuts::Action::Duplicate:
    case Shortcuts::Action::Delete:
    case Shortcuts::Action::Rename:
        if (ctx.isValid(ctx.focused()) && ctx.focused() != sceneNode && !hierarchyPanelUI.ownsFocus(ctx))
        {
            shortcut = Shortcuts::Action::None;
        }
        break;

    default:
        break;
    }

    const auto changes = session.takeEvents();

    const bool playStopped = std::any_of(changes.begin(), changes.end(),
    [](const Scribe::ChangeEvent &event) {
            return event.type == Scribe::ChangeType::PlayStopped;
    });

    if (playStopped)
        restoreSceneView(engine);

    missingCheckPending =
    missingCheckPending || checkedMissingGeneration != inputGeneration ||
    std::any_of(
        changes.begin(), changes.end(),
        [](const Scribe::ChangeEvent &event) {
            return event.type == Scribe::ChangeType::SceneReplaced ||
                Scribe::isAssetChange(event.type);
        });
    
    hierarchyPanelUI.update(ctx, engine, session, changes);

    if (restoreTabSelection)
    {
        hierarchyPanelUI.selectEntities(ctx, sceneTabs[activeTab].selection);
        restoreTabSelection = false;
    }
    
    const bool assetsChanged = std::any_of(changes.begin(), changes.end(), [](const Scribe::ChangeEvent &event) {
        return Scribe::isAssetChange(event.type);
    });

    if (!pendingEntitySelection.empty())
    {
        const auto *scene = engine.getActiveScene();

        if (pendingEntitySelectionGeneration == engine.getSceneGeneration() && !engine.isPlaying() && scene)
        {
            std::erase_if(pendingEntitySelection, [&](Scene::EntityId id) {
                return !scene->isEntityAlive(id);
            });

            hierarchyPanelUI.selectEntities(ctx, std::move(pendingEntitySelection));
        }

        pendingEntitySelection.clear();
    }

    uiEvents.clear();
    octogui::UIEvent event;
    while (ctx.pollEvent(event))
    {
        uiEvents.push_back(event);
    }

    const bool wasPlaying = engine.isPlaying();
    const bool togglePlay = shortcut == Shortcuts::Action::TogglePlay;

    const bool playMainScene = ctx.isClicked(playButton.button) || togglePlay;
    const bool playCurrentScene = ctx.isClicked(playCurrentSceneButton.button) ||
                                  shortcut == Shortcuts::Action::PlayCurrentScene;
    if ((playMainScene || playCurrentScene) && !wasPlaying && engine.hasActiveScene() &&
        !assetDrag.asset.isValid() && !assetDrag.ownsInput &&
        !restoreTabSelection)
    {
        if (!viewportPanelUI.finishGizmoDrag(session, false))
            return;

        ctx.clearFocus();
        while (ctx.pollEvent(event))
            uiEvents.push_back(event);

        // commit pending Inspector values before capturing the Play scene
        inspectorUI.update(ctx, engine, session, engine.getActiveScene(), hierarchyPanelUI.selectedEntity(),
                    uiEvents, inputGeneration, assetsChanged);

        uiEvents.clear();
        session.endMerge();
        rememberSceneView(engine);

        startScenePlay(ctx, engine, session, playCurrentScene);
        if (playDialogOpen)
            return;
    }

    if (ctx.isClicked(pauseButton.button) && engine.isPlaying())
    {
        const bool paused = engine.getSimulationState() == SimulationState::Paused;
        engine.setSimulationState(paused ? SimulationState::Running : SimulationState::Paused);
    }

    if ((ctx.isClicked(stopButton.button) || togglePlay) && wasPlaying)
    {
        if (!engine.stop())
        {
            contentBrowserUI.reportError(ctx, "Could not restore the edit scene. Play is still active.");
        }
        else
        {
            restoreSceneView(engine);
            ctx.clearFocus();
            ctx.clearEvents();
            uiEvents.clear();
        }
    }

    const bool playing = engine.isPlaying();

    if (!assetDrag.asset.isValid() && !assetDrag.ownsInput)
        viewportPanelUI.update(ctx, !playing && !restoreTabSelection);

    if (!playing && !restoreTabSelection && !assetDrag.asset.isValid() && !assetDrag.ownsInput)
    {
        if (ctx.input().mouse.pressed(MouseButton::Left))
        {
            viewportPanelUI.updateGizmo(ctx, engine, session, hierarchyPanelUI, sceneNode);
        }

        viewportPanelUI.updateMarquee(ctx, sceneNode, !engine.isPlaying() && engine.hasActiveScene(), engine.getSceneGeneration());

        viewportPanelUI.updatePicking(ctx, engine, hierarchyPanelUI, sceneNode);
    }

    if (engine.isPlaying())
        engine.getRenderer().setOutlineTargets({});
    else
        engine.getRenderer().setOutlineTargets(hierarchyPanelUI.selection());

    inspectorUI.update(ctx, engine, session, engine.getActiveScene(), 
                        hierarchyPanelUI.selectedEntity(),
                        uiEvents, inputGeneration, assetsChanged);

    if (updateFileMenu(ctx, engine, session) || updateEditViewMenus(ctx, engine, session) || updateProjectMenu(ctx, engine, session))
        return;

    if (shortcut != Shortcuts::Action::None && inputGeneration == engine.getSceneGeneration() && executeCommand(ctx, engine, session, shortcut))
    {
        return;
    }

    // commit Inspector edits before asset operations can clear history
    updateAssetDrag(ctx, engine, session);

    contentBrowserUI.update(ctx, engine, session, assetsChanged, assetDrag.ownsInput);

    if (beginAssetDelete(ctx, engine, session))
        return;

   if (!playing && !restoreTabSelection && !assetDrag.asset.isValid() && !assetDrag.ownsInput)
    {
        viewportPanelUI.updateGizmo(ctx, engine, session, hierarchyPanelUI, sceneNode);
        viewportPanelUI.drawGizmoFeedback(ctx);
    }

    updateSceneTabs(ctx, engine, session);

    const bool canRepair =
    engine.hasActiveScene() && !playing &&
    !session.transformEditActive() &&
    !assetDrag.asset.isValid() && !assetDrag.ownsInput &&
    !restoreTabSelection && !exitDialog.isOpen(ctx) &&
    !ctx.contextMenuOwnsInput() && !closingAllScenes;

    ctx.setEnabled(missingAssetsButton, canRepair);

    const bool manualRepair = ctx.isClicked(missingAssetsButton);
    const auto focus = ctx.focused();
    const bool typing = ctx.isValid(focus) && (ctx.tree().type(focus) == NodeType::TextInput || ctx.tree().type(focus) == NodeType::NumericInput);

    const auto &mouse = ctx.input().mouse;
    if (canRepair && (manualRepair || (missingCheckPending && !typing)) && !mouse.isDown(MouseButton::Left) && !mouse.isDown(MouseButton::Right) &&
        !mouse.isDown(MouseButton::Middle))
    {
        missingCheckPending = false;
        checkedMissingGeneration = engine.getSceneGeneration();
        beginReferenceRepair(ctx, engine, session, manualRepair);
    }

}
} // namespace Cthulhu::Editor
