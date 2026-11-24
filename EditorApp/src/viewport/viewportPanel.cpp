#include <algorithm>
#include <cmath>

#include <gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

#include "engine.hpp"
#include "hierarchyPanel.hpp"
#include "input.hpp"
#include "pickingMath.hpp"
#include "viewportPanel.hpp"
#include "editorMetrics.hpp"
#include "editorShortcuts.hpp"
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

constexpr Color SnapMoveColor = Color::fromRGBA8(235, 96, 96);
constexpr Color SnapRotateColor = Color::fromRGBA8(120, 215, 100);
constexpr Color SnapScaleColor = Color::fromRGBA8(96, 150, 245);

void styleToggle(Context &ctx, NodeHandle button, NodeHandle icon, bool on)
{
    const Theme &theme = ctx.theme();
    auto style = ctx.style(button);
    style.normal.background = on ? theme.accent.withAlpha(0.25f) : Color::transparent();
    style.normal.border = Color::transparent();
    style.hovered.background = on ? theme.accent.withAlpha(0.35f) : theme.surfaceHovered;
    style.active.background = on ? theme.accent.withAlpha(0.45f) : theme.surfaceActive;
    ctx.setIconColor(icon, on ? theme.focus : theme.text);
}

void styleSegment(Context &ctx, NodeHandle button, bool on)
{
    const Theme &theme = ctx.theme();
    auto style = ctx.style(button);
    style.normal.background = on ? theme.accent.withAlpha(0.25f) : Color::transparent();
    style.normal.text = on ? theme.focus : theme.textMuted;
    style.hovered.background = on ? theme.accent.withAlpha(0.35f) : theme.surfaceHovered;
    style.hovered.text = on ? theme.focus : theme.text;
    style.active.background = on ? theme.accent.withAlpha(0.45f) : theme.surfaceActive;
}

NodeHandle makeMutedLabel(Context &ctx, NodeHandle parent, std::string_view text)
{
    const NodeHandle label = ctx.createLabel(parent, text);
    ctx.style(label).all.text = ctx.theme().textMuted;
    return label;
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
    layout.minSize = Vec2{ToolButtonWidth, ControlHeight};
    layout.preferredSize = Vec2{ToolButtonWidth, ControlHeight};
    layout.maxSize = Vec2{ToolButtonWidth, ControlHeight};
    ctx.style(result.button).borderWidth = 0.0f;
    result.icon = ctx.createIcon(result.button, iconName);
    ctx.setIconSize(result.icon, IconSize);
    return result;
}

NodeHandle makeSnapIcon(Context &ctx, NodeHandle parent, std::string_view name, Color color)
{
    const NodeHandle icon = ctx.createIcon(parent, name);
    ctx.setIconSize(icon, IconSize);
    ctx.setIconColor(icon, color);

    auto layout = ctx.layoutStyle(icon);
    layout.minSize = Vec2{IconSize, IconSize};
    layout.preferredSize = Vec2{IconSize, IconSize};
    layout.maxSize = Vec2{IconSize, IconSize};
    return icon;
}

NodeHandle makeSnapInput(Context &ctx, NodeHandle parent, float value, float step)
{
    NodeHandle input = ctx.createNumericInput(parent, value, NumericInputType::Float);
    ctx.setNumericRange(input, 0.0, 1000.0);
    ctx.setNumericStep(input, step);
    auto layout = ctx.layoutStyle(input);
    layout.padding = Padding{SpaceS, SpaceXs};
    layout.minSize = Vec2{54.0f, ControlHeight};
    layout.preferredSize = Vec2{54.0f, ControlHeight};
    layout.maxSize = Vec2{54.0f, ControlHeight};
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
    headerLayout.minSize = Vec2{0.0f, ToolbarHeight};
    headerLayout.preferredSize = Vec2{0.0f, ToolbarHeight};
    headerLayout.maxSize = Vec2{InfiniteLayoutSize, ToolbarHeight};

    const NodeHandle pill = ctx.createContainer(header);
    auto pillLayout = ctx.layoutStyle(pill);
    pillLayout.mode = LayoutMode::Horizontal;
    pillLayout.horizontalAlignment = Alignment::Start;
    pillLayout.verticalAlignment = Alignment::Center;
    pillLayout.gap = SpaceS;
    pillLayout.padding = Padding{SpaceS, SpaceXs, SpaceS, SpaceXs};
    pillLayout.minSize = Vec2{0.0f, ToolbarHeight};
    pillLayout.preferredSize = Vec2{0.0f, ToolbarHeight};
    pillLayout.maxSize = Vec2{InfiniteLayoutSize, ToolbarHeight};
    {
        auto pillStyle = ctx.style(pill);
        pillStyle.all.background = ctx.theme().surfaceRaised;
        pillStyle.cornerRadius = 0.0f;
        pillStyle.borderWidth = 0.0f;
        pillStyle.shadow = ShadowStyle{};
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
        layout.minSize = Vec2{1.0f, IconSize};
        layout.preferredSize = Vec2{1.0f, IconSize};
        layout.maxSize = Vec2{1.0f, IconSize};
    }

    const NodeHandle spaceGroup = ctx.createContainer(pill);
    {
        auto layout = ctx.layoutStyle(spaceGroup);
        layout.mode = LayoutMode::Horizontal;
        layout.verticalAlignment = Alignment::Center;
        layout.padding = Padding{1.0f};
        layout.gap = 0.0f;
        layout.clip = true;
        const Vec2 size{2.0f * 48.0f + 1.0f + 2.0f, ControlHeight};
        layout.minSize = size;
        layout.preferredSize = size;
        layout.maxSize = size;

        auto style = ctx.style(spaceGroup);
        style.all.background = ctx.theme().surface;
        style.all.border = ctx.theme().border;
        style.borderWidth = 1.0f;
        style.cornerRadius = ctx.theme().controlRadius;
    }

    const auto makeSegment = [&](std::string_view text) {
        const NodeHandle button = ctx.createButton(spaceGroup, text);
        auto layout = ctx.layoutStyle(button);
        layout.minSize = Vec2{48.0f, ControlHeight - 2.0f};
        layout.preferredSize = Vec2{48.0f, ControlHeight - 2.0f};
        layout.maxSize = Vec2{48.0f, ControlHeight - 2.0f};
        layout.horizontalAlignment = Alignment::Center;
        layout.verticalAlignment = Alignment::Center;
        auto style = ctx.style(button);
        style.borderWidth = 0.0f;
        style.cornerRadius = 0.0f;
        return button;
    };

    worldButton = makeSegment("World");
    {
        NodeHandle divider = ctx.createSeparator(spaceGroup);
        ctx.setSeparatorOrientation(divider, SeparatorOrientation::Vertical);
        auto layout = ctx.layoutStyle(divider);
        layout.minSize = Vec2{1.0f, ControlHeight - 2.0f};
        layout.preferredSize = Vec2{1.0f, ControlHeight - 2.0f};
        layout.maxSize = Vec2{1.0f, ControlHeight - 2.0f};
    }
    localButton = makeSegment("Local");

    (void)ctx.createContainer(pill);

    (void)makeMutedLabel(ctx, pill, "Snapping");

    snapMoveIcon = makeSnapIcon(ctx, pill, "move", SnapMoveColor);
    snapMoveInput = makeSnapInput(ctx, pill, snapMoveValue, 0.1);
    (void)makeMutedLabel(ctx, pill, "m");

    snapRotateIcon = makeSnapIcon(ctx, pill, "rotation", SnapRotateColor);
    snapRotateInput = makeSnapInput(ctx, pill, snapRotateValue, 1.0);
    (void)makeMutedLabel(ctx, pill, "\xC2\xB0");

    snapScaleIcon = makeSnapIcon(ctx, pill, "scale", SnapScaleColor);
    snapScaleInput = makeSnapInput(ctx, pill, snapScaleValue, 0.05);
    (void)makeMutedLabel(ctx, pill, "\xC3\x97");

    refreshToggles(ctx);
}

void ViewportPanel::update(Context &ctx, bool editing)
{
    ctx.setVisible(header, editing);

    if (!editing)
    {
        gizmoSystem.vertices.clear();
        gizmoSystem.parts.clear();
        gizmoSystem.hovered = GizmoHandle::None;
        gizmoSystem.frame = {};

        gizmoCaptured = false;
        marqueePending = false;
        marqueeActive = false;
        completedMarquee.reset();
        completedClick.reset();
        return;
    }

    if (!gizmoDragActive())
    {
        switch (Shortcuts::read(ctx))
        {
        case Shortcuts::Action::SelectTool:
            setMode(ctx, GizmoMode::Select);
            break;

        case Shortcuts::Action::MoveTool:
            setMode(ctx, GizmoMode::Move);
            break;

        case Shortcuts::Action::RotateTool:
            setMode(ctx, GizmoMode::Rotate);
            break;

        case Shortcuts::Action::ScaleTool:
            setMode(ctx, GizmoMode::Scale);
            break;

        default:
            break;
        }
    }

    snapActive = Cthulhu::Core::Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) || Cthulhu::Core::Input::isKeyDown(GLFW_KEY_RIGHT_CONTROL);

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

    if (!enabled || ctx.contextMenuOwnsInput() ||  mouse.isDown(MouseButton::Right) ||
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
    draw.addRectFilled(box, ctx.theme().focus.withAlpha(0.14f));
    draw.addRectOutline(box, ctx.theme().focus, 1.0f);
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
        const auto viewport = ctx.tree().rect(sceneNode);
        const auto &startViewport = drag.viewport;
        const bool resized = viewport.x != startViewport.x || viewport.y != startViewport.y ||
                             viewport.w != startViewport.w || viewport.h != startViewport.h;

        const bool cancel = ctx.contextMenuOwnsInput() || resized || !session.transformEditActive() || engine.isPlaying() ||
                            !engine.getWindow() ||
                            glfwGetWindowAttrib(engine.getWindow()->getWindow(), GLFW_FOCUSED) != GLFW_TRUE ||
                            engine.getSceneGeneration() != drag.generation ||
                            hierarchy.selection() != drag.selection ||
                            currentMode != drag.mode || currentSpace != drag.space ||
                            Core::Input::isKeyPressed(GLFW_KEY_ESCAPE) || mouse.isDown(MouseButton::Right) ||
                            (!mouse.isDown(MouseButton::Left) && !mouse.released(MouseButton::Left));

        if (cancel)
        {
            (void)finishGizmoDrag(session, false);
        }
        else
        {
            if (drag.mode == GizmoMode::Move && !updateMoveDrag(ctx, session))
            {
                (void)finishGizmoDrag(session, false);
            }
            if (drag.handle != GizmoHandle::None && drag.mode == GizmoMode::Rotate &&
                !updateRotateDrag(ctx, session))
            {
                (void)finishGizmoDrag(session, false);
            }
            if (drag.handle != GizmoHandle::None && drag.mode == GizmoMode::Scale &&
                !updateScaleDrag(ctx, session))
            {
                (void)finishGizmoDrag(session, false);
            }

            if (drag.handle != GizmoHandle::None && mouse.released(MouseButton::Left))
            {
                (void)finishGizmoDrag(session, true);
            }
        }
    }

    const GizmoHandle previous = gizmoSystem.hovered;

    const glm::mat3 *basisOverride = nullptr;
    if (gizmoDragActive() && drag.mode == GizmoMode::Scale)
    {
        basisOverride = &drag.frame.basis;
    }

    gizmoSystem.update(engine,
                       hierarchy.selection(),
                       currentMode,
                       currentSpace,
                       ctx.tree().rect(sceneNode).h,
                       basisOverride);

    if (drag.handle == GizmoHandle::None)
    {
        const Rect rect = ctx.tree().rect(sceneNode);
        const NodeHandle hovered = ctx.hovered();

        if (ctx.contextMenuOwnsInput() || gizmoCaptured || Core::Input::isKeyPressed(GLFW_KEY_ESCAPE) || !gizmoSystem.frame.visible ||
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
                KalaHeaders::KalaLog::Log::Print(result.error, "Gizmo", KalaHeaders::KalaLog::LogType::LOG_ERROR);
                return;
            }

            drag.handle = gizmoSystem.hovered;
            drag.mode = currentMode;
            drag.space = currentSpace;
            drag.frame = gizmoSystem.frame;
            drag.viewProjection = viewProjection;
            drag.translation = {};
            drag.cameraBasis = glm::transpose(glm::mat3(camera->getViewMatrix()));
            drag.mouseStart = mouse.position;
            drag.generation = engine.getSceneGeneration();
            drag.selection = hierarchy.selection();

            const auto start = session.transformStart();
            drag.values.assign(start.begin(), start.end());

            if (drag.mode == GizmoMode::Move && !beginLinearDrag(engine, inverseViewProjection, rect))
            {
                (void)finishGizmoDrag(session, false);
                return;
            }
            if (drag.mode == GizmoMode::Rotate && !beginRotateDrag(engine, inverseViewProjection, rect))
            {
                (void)finishGizmoDrag(session, false);
                return;
            }
            if (drag.mode == GizmoMode::Scale && !beginScaleDrag(engine, inverseViewProjection, rect))
            {
                (void)finishGizmoDrag(session, false);
                return;
            }
        }
    }
    else
    {
        gizmoSystem.hovered = drag.handle;
    }

    gizmoSystem.applyHighlight(gizmoDragActive());
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

    styleSegment(ctx, worldButton, currentSpace == GizmoSpace::World);
    styleSegment(ctx, localButton, currentSpace == GizmoSpace::Local);

    // The active tool's snap icon stays bright and the others dim, so the
    // colour reads as "this tool's snap" rather than an axis.
    const auto snapColor = [&](Color color, GizmoMode mode) {
        return currentMode == mode || currentMode == GizmoMode::Select ? color : color.withAlpha(0.4f);
    };
    ctx.setIconColor(snapMoveIcon, snapColor(SnapMoveColor, GizmoMode::Move));
    ctx.setIconColor(snapRotateIcon, snapColor(SnapRotateColor, GizmoMode::Rotate));
    ctx.setIconColor(snapScaleIcon, snapColor(SnapScaleColor, GizmoMode::Scale));
}

} // namespace Cthulhu::Editor
