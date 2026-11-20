#include <GLFW/glfw3.h>

#include <iostream>
#include <string_view>

#include "editorShell.hpp"
#include "editorUi.hpp"
#include "engine.hpp"
#include "input.hpp"
#include "log_utils.hpp"
#include "session.hpp"
#include "window.hpp"
#include "editorShortcuts.hpp"

static bool isFullscreen = false;
static bool isVSyncEnabled = true;

static const float scrollSensitivity = 5.0f;

static Cthulhu::Scene::Camera *camera = nullptr;
static Cthulhu::Editor::EditorUi *editorUi = nullptr;
static bool isCameraLooking = false;
static const Cthulhu::Editor::ViewportPanel *viewportPanel = nullptr;

void onUpdate([[maybe_unused]] void *context, [[maybe_unused]] float deltaTime)
{
    Cthulhu::Engine *engine = static_cast<Cthulhu::Engine *>(context);

    const bool sceneHasMouse = !editorUi || !editorUi->wantsMouse();

    const auto shortcut = editorUi ? Cthulhu::Editor::Shortcuts::read(editorUi->context()) : Cthulhu::Editor::Shortcuts::Action::None;

    if (shortcut == Cthulhu::Editor::Shortcuts::Action::ToggleFullscreen)
    {
        isFullscreen = !isFullscreen;

        if (isFullscreen)
        {
            engine->getWindow()->setWindowMode(Cthulhu::Core::WindowMode::ExclusiveFullscreen);

            KalaHeaders::KalaLog::Log::Print("Switched to Fullscreen Mode",
                                             "Editor",
                                             KalaHeaders::KalaLog::LogType::LOG_INFO);
        }
        else
        {
            engine->getWindow()->setWindowMode(Cthulhu::Core::WindowMode::Windowed);

            KalaHeaders::KalaLog::Log::Print("Switched to Windowed Mode",
                                             "Editor",
                                             KalaHeaders::KalaLog::LogType::LOG_INFO);
        }
    }

    if (shortcut == Cthulhu::Editor::Shortcuts::Action::ToggleVSync)
    {
        isVSyncEnabled = !isVSyncEnabled;

        glfwSwapInterval(isVSyncEnabled ? 1 : 0);

        KalaHeaders::KalaLog::Log::Print(isVSyncEnabled ? "VSync Enabled" : "VSync Disabled",
                                         "Editor",
                                         KalaHeaders::KalaLog::LogType::LOG_INFO);
    }

    if (engine->getWorldMode() == Cthulhu::WorldMode::Play)
    {
        isCameraLooking = false;
        return;
    }

    GLFWwindow *window = engine->getWindow()->getWindow();

    const bool gizmoDragging = viewportPanel && viewportPanel->gizmoDragActive();
    const float scrollDeltaY = Cthulhu::Core::Input::getScrollDeltaY();

    const bool rightMouseDown = Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_2);

    const bool rightMousePressed = Cthulhu::Core::Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_2);

    const bool alreadyLooking = isCameraLooking && glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;

    const bool shouldLook = camera && rightMouseDown && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE &&
                            (!editorUi || !editorUi->context().contextMenuOwnsInput()) && (alreadyLooking || (sceneHasMouse && rightMousePressed));

    const int cursorModeWanted = shouldLook ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;

    if (shouldLook != isCameraLooking || glfwGetInputMode(window, GLFW_CURSOR) != cursorModeWanted)
    {
        glfwSetInputMode(window, GLFW_CURSOR, cursorModeWanted);
        Cthulhu::Core::Input::resetMouseTracking();
        isCameraLooking = shouldLook;
    }

    if (isCameraLooking)
    {
        camera->processMouse(Cthulhu::Core::Input::getMouseDeltaX(), Cthulhu::Core::Input::getMouseDeltaY());

        camera->processKeyboard(deltaTime);

        if (scrollDeltaY != 0.0f && !gizmoDragging)
        {
            camera->addSpeed(scrollDeltaY * scrollSensitivity);
        }
    }

    if (scrollDeltaY != 0.0f && !gizmoDragging && camera && sceneHasMouse &&
        !Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_2))
    {
        camera->setPosition(camera->getPosition() + camera->getFront() * scrollDeltaY * scrollSensitivity);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <project-path> [scene-path]\n";
        return 1;
    }

    Cthulhu::Engine engine;

    if (!engine.init(argv[1]))
    {
        KalaHeaders::KalaLog::Log::Print("FAILED TO INITIALIZE PROJECT",
                                         "Editor",
                                         KalaHeaders::KalaLog::LogType::LOG_ERROR);

        return 1;
    }

    glfwMakeContextCurrent(engine.getWindow()->getWindow());

    glfwSwapInterval(1);
    isVSyncEnabled = true;

    const auto *project = engine.getProject();
    const auto *mainScene = project ? project->getMainScene(engine.getAssetManager().getRegistry()) : nullptr;

    bool sceneReady = false;
    if (argc > 2)
        sceneReady = engine.loadScene(argv[2]);
    else
        sceneReady = (mainScene && engine.loadScene(*mainScene)) || engine.createEmptyScene("Untitled");

    if (!sceneReady)
    {
        KalaHeaders::KalaLog::Log::Print("FAILED TO OPEN AN EDITOR SCENE", "Editor", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        engine.shutdown();
        return 1;
    }

    engine.setWorldMode(Cthulhu::WorldMode::Edit);

    Cthulhu::Editor::EditorUi ui;

    if (!ui.init(engine.getWindow()->getWindow(), engine.getEngineResourceRoot()))
    {
        engine.shutdown();
        return -1;
    }

    editorUi = &ui;

    Cthulhu::Editor::EditorShell shell;
    shell.build(ui.context());
    viewportPanel = &shell.viewport();

    ui.setSceneNode(shell.sceneArea());

    Cthulhu::Scribe::Session session(engine);

    glfwSetInputMode(engine.getWindow()->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    glfwSetMouseButtonCallback(engine.getWindow()->getWindow(), nullptr);

    camera = engine.getCamera();

    while (shell.frame(ui, engine, session, onUpdate))
    {}

    shell.clearAssetDrag(engine);
    (void)session.finishTransformEdit(false);
    viewportPanel = nullptr;
    editorUi = nullptr;

    shell.shutdown(ui.context());
    ui.shutdown();
    engine.shutdown();

    return 0;
}
