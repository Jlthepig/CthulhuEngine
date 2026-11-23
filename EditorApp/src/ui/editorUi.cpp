#include "editorUi.hpp"

#include <cmath>

#include "OctoGui_GLFW.h"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
namespace
{
constexpr octogui::u32 EditorFontSize = 14;
} 

bool EditorUi::init(GLFWwindow *glfwWindow, const std::filesystem::path &resourceRoot)
{
    window = glfwWindow;

    if (!renderer.init())
    {
        Log::Print("FAILED TO INITIALIZE OCTOGUI RENDERER", "Editor", LogType::LOG_ERROR);
        return false;
    }

    ctx.setTextureBackend(&renderer);
    ctx.setTheme(octogui::darkTheme());

    const auto fontRoot = resourceRoot / "editor" / "fonts";
    const auto bodyFont = (fontRoot / "Roboto-Regular.ttf").string();
    const auto headingFont = (fontRoot / "Roboto-SemiBold.ttf").string();
    if (!ctx.loadDefaultFont(bodyFont, EditorFontSize) ||
        !ctx.loadFont(octogui::FontSlot::Secondary, headingFont, EditorFontSize))
    {
        Log::Print("FAILED TO LOAD EDITOR FONTS: " + bodyFont + " / " + headingFont, "Editor", LogType::LOG_ERROR);
        ctx.font().shutdown();
        ctx.font(octogui::FontSlot::Secondary).shutdown();
        ctx.setTextureBackend(nullptr);
        renderer.shutdown();
        return false;
    }

    const auto icons = (resourceRoot / "editor" / "icons" / "octogui_icons_v2.png").string();
    if (!ctx.loadDefaultIconAtlas(icons))
    {
        Log::Print("EDITOR ICONS NOT LOADED: " + icons, "Editor", LogType::LOG_WARNING);
    }

    (void) ctx.loadIcon("Cthulhu",
                       (resourceRoot / "editor" / "icons" / "cthulhu.png").string(),
                       octogui::IconColorMode::Original);

    (void) ctx.loadIcon("FileFolder", (resourceRoot / "editor" / "icons" / "folder_icon.png").string(), 
                        octogui::IconColorMode::Original);

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
    ctx.font().shutdown();
    ctx.font(octogui::FontSlot::Secondary).shutdown();
    ctx.setTextureBackend(nullptr);
    renderer.shutdown();
    initialized = false;
}

Shortcuts::Action EditorUi::beginFrame(float deltaTime, bool suppressInput)
{
    octogui::glfw::updateInput(ctx, window);
    auto &input = ctx.input();
    const auto capturedShortcut = Shortcuts::read(input);
    const bool mouseDown =
        input.mouse.isDown(octogui::MouseButton::Left) ||
        input.mouse.isDown(octogui::MouseButton::Right) ||
        input.mouse.isDown(octogui::MouseButton::Middle);

    if (suppressInput)
        waitForMouseRelease = true;
    else if (!mouseDown)
        waitForMouseRelease = false;

    if (suppressInput || waitForMouseRelease)
    {
        input.events.clear();
        input.modifiers.clear();
        input.mouse.down.fill(false);
        input.mouse.previousDown.fill(false);
        input.mouse.wheel = {};
        input.mouse.position = {-1.0f, -1.0f};
        input.mouse.previousPosition = input.mouse.position;
    }

    ctx.beginFrame(deltaTime);
    ctx.layout();

    const bool overScene = ctx.isValid(sceneNode) && ctx.tree().rect(sceneNode).contains(ctx.input().mouse.position);

    const octogui::NodeHandle hovered = ctx.hovered();
    const bool overControl = ctx.isValid(hovered) && hovered != ctx.root() && hovered != sceneNode;

    mouseOverUi = !overScene || overControl;

    if (suppressInput)
        return capturedShortcut == Shortcuts::Action::TogglePlay ? capturedShortcut : Shortcuts::Action::None;

    return waitForMouseRelease ? Shortcuts::Action::None : Shortcuts::read(ctx);
}

void EditorUi::endFrame(std::span<const octogui::DrawCommand> overlay)
{
    ctx.paint();
    ctx.drawList().append(overlay);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    renderer.render(ctx.drawList(),
                    ctx.input().displaySize,
                    octogui::Vec2{static_cast<octogui::f32>(framebufferWidth),
                                  static_cast<octogui::f32>(framebufferHeight)});

    ctx.endFrame();
}

PixelRect EditorUi::pixelRect(octogui::NodeHandle node) const
{
    const octogui::Rect rect = ctx.tree().rect(node);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    const octogui::Vec2 display = ctx.input().displaySize;
    const float scaleX = display.x > 0.0f ? static_cast<float>(framebufferWidth) / display.x : 1.0f;
    const float scaleY = display.y > 0.0f ? static_cast<float>(framebufferHeight) / display.y : 1.0f;
    
    const int left = static_cast<int>(std::round(rect.x * scaleX));
    const int top = static_cast<int>(std::round(rect.y * scaleY));
    const int right = static_cast<int>(std::round((rect.x + rect.w) * scaleX));
    const int bottom = static_cast<int>(std::round((rect.y + rect.h) * scaleY));
    return {left, top, right - left, bottom - top};
}

bool EditorUi::wantsKeyboard() const noexcept
{
    return ctx.contextMenuOwnsInput() || ctx.isValid(ctx.focused());
}
} // namespace Cthulhu::Editor
