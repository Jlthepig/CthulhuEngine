#include "viewportPanel.hpp"

#include "input.hpp"

#include <GLFW/glfw3.h>

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

    NodeHandle snappingHeader = ctx.createLabel(pill, " Snapping: ");

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
    const bool canShortcut = !ctx.isValid(ctx.focused()) || (!ctx.isFocused(snapMoveInput) && !ctx.isFocused(snapRotateInput) && !ctx.isFocused(snapScaleInput));
    if (canShortcut)
    {
        if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_Q)) setMode(ctx, GizmoMode::Select);
        else if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_W)) setMode(ctx, GizmoMode::Move);
        else if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_E)) setMode(ctx, GizmoMode::Rotate);
        else if (Cthulhu::Core::Input::isKeyPressed(GLFW_KEY_R)) setMode(ctx, GizmoMode::Scale);
    }

    snapActive = Cthulhu::Core::Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) || Cthulhu::Core::Input::isKeyDown(GLFW_KEY_RIGHT_CONTROL);

    if (ctx.isClicked(selectButton)) setMode(ctx, GizmoMode::Select);
    else if (ctx.isClicked(moveButton)) setMode(ctx, GizmoMode::Move);
    else if (ctx.isClicked(rotateButton)) setMode(ctx, GizmoMode::Rotate);
    else if (ctx.isClicked(scaleButton)) setMode(ctx, GizmoMode::Scale);

    if (ctx.isClicked(worldButton)) setSpace(ctx, GizmoSpace::World);
    else if (ctx.isClicked(localButton)) setSpace(ctx, GizmoSpace::Local);

    snapMoveValue = static_cast<float>(ctx.numericValue(snapMoveInput));
    snapRotateValue = static_cast<float>(ctx.numericValue(snapRotateInput));
    snapScaleValue = static_cast<float>(ctx.numericValue(snapScaleInput));
}

void ViewportPanel::setMode(Context &ctx, GizmoMode mode)
{
    if (currentMode == mode) return;
    currentMode = mode;
    refreshToggles(ctx);
}

void ViewportPanel::setSpace(Context &ctx, GizmoSpace space)
{
    if (currentSpace == space) return;
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
