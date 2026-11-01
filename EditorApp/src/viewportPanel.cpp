#include <algorithm>
#include <cmath>

#include <gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

#include "engine.hpp"
#include "hierarchyPanel.hpp"
#include "input.hpp"
#include "pickingMath.hpp"
#include "viewportPanel.hpp"
#include "window.hpp"
#include "session.hpp"
#include "log_utils.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
namespace
{
struct IconButton
{
    NodeHandle button{};
    NodeHandle icon{};
};

void styleToggle(Context &ctx, NodeHandle button, NodeHandle icon, bool on)
{
    const Theme &theme = ctx.theme();
    auto style = ctx.style(button);
    style.normal.background = on ? theme.accent : theme.surface;
    style.normal.border = on ? theme.accentActive : theme.border;
    style.hovered.background = on ? theme.accentHovered : theme.surfaceHovered;
    style.active.background = on ? theme.accentActive : theme.surfaceActive;
    ctx.setIconColor(icon, on ? theme.accentText : theme.text);
}

IconButton makeToolButton(Context &ctx, NodeHandle parent, std::string_view iconName)
{
    IconButton result{};
    result.button = ctx.createButton(parent, "");
    auto layout = ctx.layoutStyle(result.button);
    layout.mode = LayoutMode::Horizontal;
    layout.horizontalAlignment = Alignment::Center;
    layout.verticalAlignment = Alignment::Center;
    layout.padding = Padding{};
    layout.minSize = Vec2{28.0f, 26.0f};
    layout.preferredSize = Vec2{28.0f, 26.0f};
    layout.maxSize = Vec2{28.0f, 26.0f};
    result.icon = ctx.createIcon(result.button, iconName);
    ctx.setIconSize(result.icon, 16.0f);
    return result;
}

void makeSnapIcon(Context &ctx, NodeHandle parent, std::string_view name, Color color)
{
    const NodeHandle icon = ctx.createIcon(parent, name);
    ctx.setIconSize(icon, 16.0f);
    ctx.setIconColor(icon, color);

    auto layout = ctx.layoutStyle(icon);
    layout.minSize = Vec2{16.0f, 16.0f};
    layout.preferredSize = Vec2{16.0f, 16.0f};
    layout.maxSize = Vec2{16.0f, 16.0f};
}

NodeHandle makeSnapInput(Context &ctx, NodeHandle parent, float value, float step)
{
    NodeHandle input = ctx.createNumericInput(parent, value, NumericInputType::Float);
    ctx.setNumericRange(input, 0.0, 1000.0);
    ctx.setNumericStep(input, step);
    auto layout = ctx.layoutStyle(input);
    layout.padding = Padding{6.0f, 2.0f};
    layout.minSize = Vec2{54.0f, 26.0f};
    layout.preferredSize = Vec2{54.0f, 26.0f};
    layout.maxSize = Vec2{54.0f, 26.0f};
    return input;
}
} // namespace

void ViewportPanel::build(Context &ctx, NodeHandle viewportRoot)
{
    overlayNode = ctx.createContainer(viewportRoot);
    auto overlayLayout = ctx.layoutStyle(overlayNode);
    overlayLayout.mode = LayoutMode::Vertical;
    overlayLayout.horizontalAlignment = Alignment::Stretch;
    overlayLayout.verticalAlignment = Alignment::Start;
    overlayLayout.padding = Padding{};

    header = ctx.createContainer(overlayNode);
    auto headerLayout = ctx.layoutStyle(header);
    headerLayout.mode = LayoutMode::Horizontal;
    headerLayout.horizontalAlignment = Alignment::Stretch;
    headerLayout.verticalAlignment = Alignment::Center;
    headerLayout.gap = 0.0f;
    headerLayout.padding = Padding{};
    headerLayout.minSize = Vec2{0.0f, 36.0f};
    headerLayout.preferredSize = Vec2{0.0f, 36.0f};
    headerLayout.maxSize = Vec2{InfiniteLayoutSize, 36.0f};

    pill = ctx.createContainer(header);
    auto pillLayout = ctx.layoutStyle(pill);
    pillLayout.mode = LayoutMode::Horizontal;
    pillLayout.horizontalAlignment = Alignment::Start;
    pillLayout.verticalAlignment = Alignment::Center;
    pillLayout.gap = 4.0f;
    pillLayout.padding = Padding{6.0f, 4.0f, 6.0f, 4.0f};
    pillLayout.minSize = Vec2{0.0f, 36.0f};
    pillLayout.preferredSize = Vec2{0.0f, 36.0f};
    pillLayout.maxSize = Vec2{InfiniteLayoutSize, 36.0f};
    {
        auto pillStyle = ctx.style(pill);
        pillStyle.all.background = ctx.theme().surface;
        pillStyle.all.border = ctx.theme().border;
        pillStyle.cornerRadius = 0.0f;
        pillStyle.borderWidth = 1.0f;
        pillStyle.shadow = ctx.theme().elevationLow;
    }

    const IconButton select = makeToolButton(ctx, pill, "cursor");
    selectButton = select.button;
    selectIcon = select.icon;

    const IconButton move = makeToolButton(ctx, pill, "move");
    moveButton = move.button;
    moveIcon = move.icon;

    const IconButton rotate = makeToolButton(ctx, pill, "rotation");
    rotateButton = rotate.button;
    rotateIcon = rotate.icon;

    const IconButton scale = makeToolButton(ctx, pill, "scale");
    scaleButton = scale.button;
    scaleIcon = scale.icon;

    {
        NodeHandle sep = ctx.createSeparator(pill);
        ctx.setSeparatorOrientation(sep, SeparatorOrientation::Vertical);
        auto layout = ctx.layoutStyle(sep);
        layout.minSize = Vec2{1.0f, 18.0f};
        layout.preferredSize = Vec2{1.0f, 18.0f};
        layout.maxSize = Vec2{1.0f, 18.0f};
    }

    worldButton = ctx.createButton(pill, "World");
    {
        auto layout = ctx.layoutStyle(worldButton);
        layout.minSize = Vec2{48.0f, 26.0f};
        layout.preferredSize = Vec2{48.0f, 26.0f};
        layout.maxSize = Vec2{48.0f, 26.0f};
        layout.horizontalAlignment = Alignment::Center;
        layout.verticalAlignment = Alignment::Center;
    }
    localButton = ctx.createButton(pill, "Local");
    {
        auto layout = ctx.layoutStyle(localButton);
        layout.minSize = Vec2{48.0f, 26.0f};
        layout.preferredSize = Vec2{48.0f, 26.0f};
        layout.maxSize = Vec2{48.0f, 26.0f};
        layout.horizontalAlignment = Alignment::Center;
        layout.verticalAlignment = Alignment::Center;
    }

    {
        NodeHandle sep = ctx.createSeparator(pill);
        ctx.setSeparatorOrientation(sep, SeparatorOrientation::Vertical);
        auto layout = ctx.layoutStyle(sep);
        layout.minSize = Vec2{1.0f, 18.0f};
        layout.preferredSize = Vec2{1.0f, 18.0f};
        layout.maxSize = Vec2{1.0f, 18.0f};
    }

    [[maybe_unused]] const NodeHandle snappingHeader = ctx.createLabel(pill, " Snapping: ");

    makeSnapIcon(ctx, pill, "move", Color::fromRGBA8(235, 96, 96));
    snapMoveInput = makeSnapInput(ctx, pill, snapMoveValue, 0.1);

    makeSnapIcon(ctx, pill, "rotation", Color::fromRGBA8(120, 215, 100));
    snapRotateInput = makeSnapInput(ctx, pill, snapRotateValue, 1.0);

    makeSnapIcon(ctx, pill, "scale", Color::fromRGBA8(96, 150, 245));
    snapScaleInput = makeSnapInput(ctx, pill, snapScaleValue, 0.05);

    refreshToggles(ctx);
}

void ViewportPanel::update(Context &ctx)
{
    const bool canShortcut =
        !ctx.isValid(ctx.focused()) ||
        (!ctx.isFocused(snapMoveInput) && !ctx.isFocused(snapRotateInput) && !ctx.isFocused(snapScaleInput));
    if (canShortcut && !Cthulhu::Core::Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
    {
        if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_Q))
            setMode(ctx, GizmoMode::Select);
        else if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_W))
            setMode(ctx, GizmoMode::Move);
        else if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_E))
            setMode(ctx, GizmoMode::Rotate);
        else if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_R))
            setMode(ctx, GizmoMode::Scale);
    }

    snapActive = Cthulhu::Core::Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
                 Cthulhu::Core::Input::isKeyDown(GLFW_KEY_RIGHT_CONTROL);

    if (ctx.isClicked(selectButton))
        setMode(ctx, GizmoMode::Select);
    else if (ctx.isClicked(moveButton))
        setMode(ctx, GizmoMode::Move);
    else if (ctx.isClicked(rotateButton))
        setMode(ctx, GizmoMode::Rotate);
    else if (ctx.isClicked(scaleButton))
        setMode(ctx, GizmoMode::Scale);

    if (ctx.isClicked(worldButton))
        setSpace(ctx, GizmoSpace::World);
    else if (ctx.isClicked(localButton))
        setSpace(ctx, GizmoSpace::Local);

    snapMoveValue = static_cast<float>(ctx.numericValue(snapMoveInput));
    snapRotateValue = static_cast<float>(ctx.numericValue(snapRotateInput));
    snapScaleValue = static_cast<float>(ctx.numericValue(snapScaleInput));
}

void ViewportPanel::updateMarquee(Context &ctx, NodeHandle sceneNode, bool enabled, std::uint64_t sceneGeneration)
{
    completedMarquee.reset();
    completedClick.reset();
    const auto &mouse = ctx.input().mouse;

    if (!enabled || mouse.isDown(MouseButton::Right) ||
    ((marqueePending || gizmoCaptured) && marqueeGeneration != sceneGeneration) ||
    Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_ESCAPE))
    {
        gizmoCaptured = false;
        marqueePending = false;
        marqueeActive = false;
        return;
    }

    if (gizmoCaptured)
    {
        if (mouse.released(MouseButton::Left) || !mouse.isDown(MouseButton::Left))
        {
            gizmoCaptured = false;
        }
        return;
    }

    const Rect scene = ctx.tree().rect(sceneNode);
    const Rect toolbar = ctx.tree().rect(header);

    const float left = scene.x;
    const float right = scene.x + scene.w;
    const float top = std::max(scene.y, toolbar.y + toolbar.h);
    const float bottom = scene.y + scene.h;

    if (right <= left || bottom <= top)
    {
        marqueePending = false;
        marqueeActive = false;
        return;
    }

    if (mouse.pressed(MouseButton::Left))
    {
        const NodeHandle hovered = ctx.hovered();
        const bool blockedByUi = ctx.isValid(hovered) && hovered != sceneNode && hovered != overlayNode;

        const Vec2 point = mouse.position;
        const bool inside = point.x >= left && point.x < right && point.y >= top && point.y < bottom;

        marqueePending = inside && !blockedByUi;
        marqueeActive = false;

        if (marqueePending)
        {
            marqueeStart = point;
            marqueeGeneration = sceneGeneration;
            selectionCtrl = ctx.input().modifiers.ctrl;
            selectionAdditive = selectionCtrl || ctx.input().modifiers.shift;
        }
    }

    if (!marqueePending)
        return;

    const Vec2 point{std::clamp(mouse.position.x, left, right), std::clamp(mouse.position.y, top, bottom)};

    const Vec2 delta = point - marqueeStart;
    constexpr float dragThreshold = 4.0f;

    if (delta.x * delta.x + delta.y * delta.y >= dragThreshold * dragThreshold)
    {
        marqueeActive = true;
    }

    const Rect box{std::min(marqueeStart.x, point.x),
                   std::min(marqueeStart.y, point.y),
                   std::max(marqueeStart.x, point.x) - std::min(marqueeStart.x, point.x),
                   std::max(marqueeStart.y, point.y) - std::min(marqueeStart.y, point.y)};

    if (mouse.released(MouseButton::Left))
    {
        if (marqueeActive)
            completedMarquee = box;
        else
            completedClick = marqueeStart;
        marqueePending = false;
        marqueeActive = false;
        return;
    }

    if (!mouse.isDown(MouseButton::Left))
    {
        marqueePending = false;
        marqueeActive = false;
        return;
    }

    if (!marqueeActive)
        return;

    auto &draw = ctx.drawList();
    draw.pushClip(Rect{left, top, right - left, bottom - top});
    draw.addRectFilled(box, Color::fromRGBA8(155, 185, 210, 45));
    draw.addRectOutline(box, Color::fromRGBA8(175, 200, 220), 1.0f);
    draw.popClip();
}

bool ViewportPanel::finishGizmoDrag(Scribe::Session &session, bool commit)
{
    const auto result = session.finishTransformEdit(commit);
    if (!result.ok())
    {
        KalaHeaders::KalaLog::Log::Print(result.error, "Gizmo", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return false;
    }

    drag.handle = GizmoHandle::None;
    return true;
}

void ViewportPanel::updateGizmo(Context &ctx, Engine &engine, Scribe::Session &session, const HierarchyPanel &hierarchy, NodeHandle sceneNode)
{
    const auto &mouse = ctx.input().mouse;

    if (drag.handle != GizmoHandle::None)
    {
        const bool cancel = !session.transformEditActive() || engine.isPlaying() ||
                            !engine.getWindow() ||
                            glfwGetWindowAttrib(engine.getWindow()->getWindow(), GLFW_FOCUSED) != GLFW_TRUE ||
                            engine.getSceneGeneration() != drag.generation ||
                            hierarchy.selection() != drag.selection ||
                            currentMode != drag.mode || currentSpace != drag.space ||
                            Core::Input::isKeyPressed(GLFW_KEY_ESCAPE) || mouse.isDown(MouseButton::Right) ||
                            (!mouse.isDown(MouseButton::Left) && !mouse.released(MouseButton::Left));

        if (cancel || mouse.released(MouseButton::Left))
        {
            (void)finishGizmoDrag(session, !cancel);
        }
    }

    const GizmoHandle previous = gizmoSystem.hovered;

    gizmoSystem.update(engine, hierarchy.selection(), currentMode, currentSpace, ctx.tree().rect(sceneNode).h);

    if (drag.handle == GizmoHandle::None)
    {
        const Rect rect = ctx.tree().rect(sceneNode);
        const NodeHandle hovered = ctx.hovered();

        if (gizmoCaptured || Core::Input::isKeyPressed(GLFW_KEY_ESCAPE) || !gizmoSystem.frame.visible ||
            mouse.isDown(MouseButton::Right) ||
            !rect.contains(mouse.position) ||
            ctx.tree().rect(header).contains(mouse.position) ||
            (ctx.isValid(hovered) && hovered != sceneNode && hovered != overlayNode))
        {
            return;
        }

        auto *camera = engine.getCamera();
        auto *window = engine.getWindow();
        if (!camera || !window)
        {
            return;
        }

        GLFWwindow *nativeWindow = window->getWindow();
        if (glfwGetInputMode(nativeWindow, GLFW_CURSOR) != GLFW_CURSOR_NORMAL)
        {
            return;
        }

        const Vec2 display = ctx.input().displaySize;
        if (rect.w <= 0.0f || rect.h <= 0.0f || display.x <= 0.0f || display.y <= 0.0f)
        {
            return;
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(nativeWindow, &framebufferWidth, &framebufferHeight);

        const int width = static_cast<int>(rect.w * static_cast<float>(framebufferWidth) / display.x);
        const int height = static_cast<int>(rect.h * static_cast<float>(framebufferHeight) / display.y);
        if (width <= 0 || height <= 0)
        {
            return;
        }

        const auto &config = engine.getRenderer().getConfig();
        const glm::mat4 projection = glm::perspective(glm::radians(camera->getFov()),
                                                    static_cast<float>(width) / static_cast<float>(height),
                                                    config.nearPlane,
                                                    config.farPlane);

        const glm::mat4 viewProjection = projection * camera->getViewMatrix();
        const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

        const float x = 2.0f * (mouse.position.x - rect.x) / rect.w - 1.0f;
        const float y = 1.0f - 2.0f * (mouse.position.y - rect.y) / rect.h;

        glm::vec4 nearPoint = inverseViewProjection * glm::vec4{x, y, -1.0f, 1.0f};
        glm::vec4 farPoint = inverseViewProjection * glm::vec4{x, y, 1.0f, 1.0f};

        if (nearPoint.w == 0.0f || farPoint.w == 0.0f)
        {
            return;
        }

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        const glm::vec3 segment = glm::vec3(farPoint - nearPoint);
        const float distance = glm::length(segment);
        if (!std::isfinite(distance) || distance <= 0.0f)
        {
            return;
        }

        gizmoSystem.hovered = gizmoSystem.pick(PickingRay{glm::vec3(nearPoint), segment / distance},distance, viewProjection,
                                    glm::vec2{rect.w, rect.h}, glm::vec2{mouse.position.x - rect.x, mouse.position.y - rect.y}, previous);

        if (mouse.pressed(MouseButton::Left) && !gizmoCaptured &&
            gizmoSystem.hovered != GizmoHandle::None)
        {
            gizmoCaptured = true;
            marqueeGeneration = engine.getSceneGeneration();

            const auto result = session.beginTransformEdit(hierarchy.selection());
            if (!result.ok())
            {
                KalaHeaders::KalaLog::Log::Print(
                    result.error, "Gizmo", KalaHeaders::KalaLog::LogType::LOG_ERROR);
                return;
            }

            drag.handle = gizmoSystem.hovered;
            drag.mode = currentMode;
            drag.space = currentSpace;
            drag.frame = gizmoSystem.frame;
            drag.cameraBasis = glm::transpose(glm::mat3(camera->getViewMatrix()));
            drag.mouseStart = mouse.position;
            drag.generation = engine.getSceneGeneration();
            drag.selection = hierarchy.selection();

            const auto start = session.transformStart();
            drag.values.assign(start.begin(), start.end());
        }
    }
    else
    {
        gizmoSystem.hovered = drag.handle;
    }

    if (gizmoSystem.hovered == GizmoHandle::None)
    {
        return;
    }

    for (const GizmoPart &part : gizmoSystem.parts)
    {
        if (part.handle != gizmoSystem.hovered ||
            part.firstVertex > gizmoSystem.vertices.size() ||
            part.vertexCount > gizmoSystem.vertices.size() - part.firstVertex)
        {
            continue;
        }

        const std::size_t end = part.firstVertex + part.vertexCount;
        for (std::size_t first = part.firstVertex; first < end; ++first)
        {
            auto &color = gizmoSystem.vertices[first].color;
            const float brightness = glm::max(color.r, glm::max(color.g, color.b));
            color = glm::vec4(gizmoSystem.style.hoverColor * brightness, color.a);
        }
    }
}

void ViewportPanel::setMode(Context &ctx, GizmoMode mode)
{
    if (currentMode == mode)
        return;
    currentMode = mode;
    refreshToggles(ctx);
}

void ViewportPanel::setSpace(Context &ctx, GizmoSpace space)
{
    if (currentSpace == space)
        return;
    currentSpace = space;
    refreshToggles(ctx);
}

void ViewportPanel::refreshToggles(Context &ctx)
{
    styleToggle(ctx, selectButton, selectIcon, currentMode == GizmoMode::Select);
    styleToggle(ctx, moveButton, moveIcon, currentMode == GizmoMode::Move);
    styleToggle(ctx, rotateButton, rotateIcon, currentMode == GizmoMode::Rotate);
    styleToggle(ctx, scaleButton, scaleIcon, currentMode == GizmoMode::Scale);

    const Theme &theme = ctx.theme();
    auto styleWorld = ctx.style(worldButton);
    styleWorld.normal.background = currentSpace == GizmoSpace::World ? theme.accent : theme.surface;
    styleWorld.normal.text = currentSpace == GizmoSpace::World ? theme.accentText : theme.text;
    styleWorld.normal.border = currentSpace == GizmoSpace::World ? theme.accentActive : theme.border;

    auto styleLocal = ctx.style(localButton);
    styleLocal.normal.background = currentSpace == GizmoSpace::Local ? theme.accent : theme.surface;
    styleLocal.normal.text = currentSpace == GizmoSpace::Local ? theme.accentText : theme.text;
    styleLocal.normal.border = currentSpace == GizmoSpace::Local ? theme.accentActive : theme.border;
}

} // namespace Cthulhu::Editor
