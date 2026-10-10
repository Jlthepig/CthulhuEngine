#include <cstdio>
#include <string>

#include <GLFW/glfw3.h>

#include "engine.hpp"
#include "input.hpp"
#include "window.hpp"
#include "log_utils.hpp"

#include "editorUi.hpp"

static bool isFullscreen = false;
static const float zoomSpeed = 350.0f;
static Cthulhu::Scene::Camera* camera = nullptr;
static Cthulhu::Editor::EditorUi* editorUi = nullptr;

void onUpdate([[maybe_unused]] void* context, [[maybe_unused]] float deltaTime)
{
    Cthulhu::Engine* engine = static_cast<Cthulhu::Engine*>(context);

    const bool sceneHasMouse = !editorUi || !editorUi->wantsMouse();
    const bool sceneHasKeyboard = !editorUi || !editorUi->wantsKeyboard();

    if (sceneHasKeyboard && Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_F11))
    {
        isFullscreen = !isFullscreen;
        if (isFullscreen)
        {
            engine->getWindow()->setWindowMode(Cthulhu::Core::WindowMode::ExclusiveFullscreen);
            KalaHeaders::KalaLog::Log::Print("Switched to Fullscreen Mode", "Editor", KalaHeaders::KalaLog::LogType::LOG_INFO);
        }
        else
        {
            engine->getWindow()->setWindowMode(Cthulhu::Core::WindowMode::Windowed);
            KalaHeaders::KalaLog::Log::Print("Switched to Windowed Mode", "Editor", KalaHeaders::KalaLog::LogType::LOG_INFO);
        }
    }

    if (sceneHasKeyboard && Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_F5))
    {
        const bool ok = engine->isPlaying() ? engine->stop() : engine->play();
        (void)ok;
    }

    float scrollDeltaY = Cthulhu::Core::Input::getScrollDeltaY();

    if (camera && sceneHasMouse && Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_2))
    {
        glfwSetInputMode(engine->getWindow()->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        camera->processMouse(Cthulhu::Core::Input::getMouseDeltaX(), Cthulhu::Core::Input::getMouseDeltaY());
        camera->processKeyboard(deltaTime);
        if (scrollDeltaY != 0.0f)
        {
           camera->addSpeed(    scrollDeltaY * zoomSpeed * deltaTime);
        }
    }
    else
    {
        glfwSetInputMode(engine->getWindow()->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    
    if (scrollDeltaY != 0.0f && camera && sceneHasMouse && !Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_2))
    {
        camera->setPosition(camera->getPosition() + camera->getFront() * scrollDeltaY * zoomSpeed * deltaTime);  
    }
    
}

int main()
{
    Cthulhu::Engine engine;
    if (!engine.init("C:\\Users\\jarri\\Desktop\\CthulhuSandbox\\project.cthulhu"))
    {
        KalaHeaders::KalaLog::Log::Print("FAILED TO INITIALIZE PROJECT", "Editor", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return 1;
    }

    if (!engine.loadScene("res://assets/scenes/test.scene"))
    {
        KalaHeaders::KalaLog::Log::Print("FAILED TO LOAD EDITOR SCENE", "Editor", KalaHeaders::KalaLog::LogType::LOG_ERROR);
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

    auto& ctx = ui.context();
    const octogui::NodeHandle root = ctx.ensureRoot();
    ctx.style(root).all.background = octogui::Color::fromRGBA8(0, 0, 0, 0);
    const octogui::NodeHandle title = ctx.createLabel(root, "CthulhuEditor");
    (void) title;

    glfwSetInputMode(engine.getWindow()->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetMouseButtonCallback(engine.getWindow()->getWindow(), nullptr);

    camera = engine.getCamera();
    engine.setUpdateCallback(onUpdate, &engine);

    while (!engine.shouldClose()) 
    {
        engine.update();
        engine.render();
        ui.beginFrame(engine.getDeltaTime());
        ui.endFrame();

        engine.present();
    }

    editorUi = nullptr;
    ui.shutdown();
    engine.shutdown();
    return 0;
}