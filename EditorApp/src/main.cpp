#include "engine.h"
#include "glfw3.h"
#include "input.h"
#include "window.h"
#include "log_utils.hpp"

static bool isFullscreen = false;
static const float zoomSpeed = 350.0f;
static Cthulhu::Scene::Camera* camera = nullptr;

void onUpdate([[maybe_unused]] void* context, [[maybe_unused]] float deltaTime)
{
    Cthulhu::Engine* engine = static_cast<Cthulhu::Engine*>(context);

    if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_F11))
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

    float scrollDeltaY = Cthulhu::Core::Input::getScrollDeltaY();

    if (camera && Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_2))
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

    
    if (scrollDeltaY != 0.0f && camera && !Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_2))
    {
        camera->setPosition(camera->getPosition() + camera->getFront() * scrollDeltaY * zoomSpeed * deltaTime);  
    }
    
}

int main()
{
    Cthulhu::Engine engine;
    engine.init("Cthulhu Engine", glm::vec2(1920.0f, 1080.0f));
    engine.loadScene("assets/scenes/test.scene");
    engine.setUpdateCallback(onUpdate,&engine);

    glfwSetInputMode(engine.getWindow()->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetMouseButtonCallback(engine.getWindow()->getWindow(), nullptr);

    camera = engine.getCamera();

    engine.run();
    engine.shutdown();

    return 0;
}