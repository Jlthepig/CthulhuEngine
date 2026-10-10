#include "editorUi.hpp"

#include "OctoGui_GLFW.h"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
bool EditorUi::init(GLFWwindow *glfwWindow, const std::filesystem::path &resourceRoot)
{
    window = glfwWindow;

    // OpenGL is already loaded by the engine OctoGUI draws with the same loader
    if (!renderer.init())
    {
        Log::Print("FAILED TO INITIALIZE OCTOGUI RENDERER", "Editor", LogType::LOG_ERROR);
        return false;
    }
    
    ctx.setTextureBackend(&renderer);
    ctx.setTheme(octogui::darkTheme());

    const auto font = (resourceRoot / "editor" / "fonts" / "Roboto-SemiBold.ttf").string();
    if (!ctx.loadDefaultFont(font, 18))
    {
        Log::Print("FAILED TO LOAD EDITOR FONT: " + font, "Editor", LogType::LOG_ERROR);
        renderer.shutdown();
        return false;
    }

    const auto icons = (resourceRoot / "editor" / "icons" / "octogui_icons.png").string();
    if (!ctx.loadDefaultIconAtlas(icons))
    {
        Log::Print("EDITOR ICONS NOT LOADED: " + icons, "Editor", LogType::LOG_WARNING);
    }

    octogui::glfw::initInput(window);

    initialized = true;
    return true;
}

void EditorUi::shutdown()
{
    if (!initialized)
    {
        return;
    }

    octogui::glfw::shutdownInput(window);
    renderer.shutdown();
    initialized = false;
}

void EditorUi::beginFrame(float deltaTime)
{
    octogui::glfw::updateInput(ctx, window);
    ctx.beginFrame(deltaTime);
    ctx.layout();

    mouseOverUi = ctx.isValid(ctx.hovered()) && !ctx.isHovered(ctx.root());
}

void EditorUi::endFrame()
{
    ctx.paint();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    renderer.render(ctx.drawList(), ctx.input().displaySize,
                    octogui::Vec2{static_cast<octogui::f32>(framebufferWidth),
                                  static_cast<octogui::f32>(framebufferHeight)});

    ctx.endFrame();
}

bool EditorUi::wantsKeyboard() const noexcept
{
    return ctx.isValid(ctx.focused());
}
} // namespace Cthulhu::Editor