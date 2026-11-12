#include "editorShell.hpp"

#include <algorithm>
#include <cassert>

#include <GLFW/glfw3.h>

#include "assetRegistry.hpp"
#include "engine.hpp"
#include "project.hpp"
#include "session.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
namespace
{
void drawAssetBadge(Context &ctx, DrawList &draw, std::string_view name, const AssetDropFeedback &feedback)
{
    const auto display = ctx.input().displaySize;
    const float textWidth = std::max(ctx.font().measureText(name).x, ctx.font().measureText(feedback.message).x);
    const float width = std::min(std::min(360.0f, display.x - 8.0f), std::max(160.0f, textWidth + 16.0f));
    const float line = ctx.font().metrics().lineHeight;
    const float height = line * 2.0f + 12.0f;
    if (width <= 0.0f || height > display.y - 8.0f) return;

    const auto mouse = ctx.input().mouse.position;
    const float x = std::clamp(mouse.x + 14.0f, 4.0f, std::max(4.0f, display.x - width - 4.0f));
    const float y = std::clamp(mouse.y + 18.0f, 4.0f, std::max(4.0f, display.y - height - 4.0f));

    const Rect rect{x, y, width, height};
    const auto color = feedback.accepted ? ctx.theme().accent : Color::fromRGBA8(224, 90, 90);

    draw.addRoundedRectFilled(rect, ctx.theme().surfaceRaised, 4.0f);
    draw.addRoundedRectOutline(rect, color, 4.0f, 1.0f);

    ctx.font().drawTextClipped(draw, name, Vec2{x + 8.0f, y + 6.0f}, ctx.theme().text, rect.shrink(4.0f));
    ctx.font().drawTextClipped(draw, feedback.message, Vec2{x + 8.0f, y + 6.0f + line}, color, rect.shrink(4.0f));
}
} // namespace

void EditorShell::clearAssetDrag(Engine &engine)
{
    if (assetDrag.previewModel.isValid())
        engine.getAssetManager().releaseModel(assetDrag.previewModel);

    assetDrag.previewModel = {};
    assetDrag.previewLoadAttempted = false;
    assetDrag.asset = {};
    assetDrag.active = false;
    assetPreviewPosition.reset();
}

AssetDropFeedback EditorShell::dropInViewport(Context &ctx, Engine &engine, Scribe::Session &session, const Assets::AssetRecord &asset, bool released)
{
    if (asset.type != Assets::AssetType::Model)
        return {false, "Only models can be placed in the viewport."};

    auto &assets = engine.getAssetManager();
    if (!assetDrag.previewLoadAttempted)
    {
        assetDrag.previewLoadAttempted = true;
        assetDrag.previewModel = assets.acquireModel(asset.path);
    }

    const auto *model = assets.getModel(assetDrag.previewModel);
    if (!model)
        return {false, "Could not load this model.", released};

    const auto position = viewportPanelUI.modelDropPosition(ctx, engine, sceneNode, *model);
    if (!position)
        return {false, "No placement point here."};

    if (!released)
    {
        assetPreviewPosition = *position;
        return {true, "Release to place model."};
    }

    const auto placed = session.placeModel(asset.id, *position);
    if (!placed.ok())
        return {false, placed.result.error, true};

    assert(placed.id.isValid());
    pendingAssetSelection = placed.id;
    pendingAssetSelectionGeneration = engine.getSceneGeneration();
    return {true, "Model placed."};
}

void EditorShell::updateAssetDrag(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto &mouse = ctx.input().mouse;
    const auto *project = engine.getProject();
    const auto *window = engine.getWindow();
    const bool focused = window && window->getWindow() && glfwGetWindowAttrib(window->getWindow(), GLFW_FOCUSED) == GLFW_TRUE;

    const bool escape = std::any_of(ctx.input().events.begin(), ctx.input().events.end(), [](const InputEvent &event) {
        return event.type == InputEventType::Key && event.action == KeyAction::Press && event.key == Key::Escape;
    });

    const bool cancelled = !project || !focused ||
        engine.isPlaying() || session.transformEditActive() ||
        ctx.contextMenuOwnsInput() || escape ||
        mouse.isDown(MouseButton::Right) ||
        (assetDrag.asset.isValid() &&
         (assetDrag.generation != engine.getSceneGeneration() ||
          assetDrag.projectFile != project->getProjectFilePath()));

    if (cancelled)
    {
        assetDrag.ownsInput = assetDrag.ownsInput || assetDrag.asset.isValid();
        clearAssetDrag(engine);
        return;
    }

    if (!assetDrag.asset.isValid() && !assetDrag.ownsInput && mouse.pressed(MouseButton::Left))
    {
        assetDrag.asset = contentBrowserUI.beginAssetDrag(ctx, *project);
        if (assetDrag.asset.isValid())
        {
            assetDrag.projectFile = project->getProjectFilePath();
            assetDrag.generation = engine.getSceneGeneration();
            assetDrag.press = mouse.position;
        }
    }
    if (!assetDrag.asset.isValid()) return;

    const auto *asset = engine.getAssetManager().getRegistry().findById(assetDrag.asset);

    if (!asset || asset->missing || (!mouse.isDown(MouseButton::Left) && !mouse.released(MouseButton::Left)))
    {
        clearAssetDrag(engine);
        assetDrag.ownsInput = true;
        return;
    }

    const Vec2 delta = mouse.position - assetDrag.press;
    if (delta.x * delta.x + delta.y * delta.y >= 25.0f)
        assetDrag.active = true;

    if (assetDrag.active)
    {
        assetDrag.ownsInput = true;
        assert(assetDrag.asset.isValid());

        const std::string_view path(asset->path);
        const auto slash = path.find_last_of('/');
        assetDrag.name.assign(path.substr(slash == std::string_view::npos ? 0 : slash + 1));

        ctx.resolvePendingLayout();

        const bool released = mouse.released(MouseButton::Left);
        auto feedback = contentBrowserUI.folderDrop(ctx, engine, session, *asset, released, assetFeedback);
        if (!feedback)
        {
            feedback = engine.hasActiveScene() && viewportPanelUI.isAssetDropTarget(ctx, sceneNode)
                           ? dropInViewport(ctx, engine, session, *asset, released)
                           : inspectorUI.assetDrop(ctx, engine, session, *asset, released, assetFeedback);
        }

        drawAssetBadge(ctx, assetFeedback, assetDrag.name, *feedback);

        if (feedback->failed)
            contentBrowserUI.reportError(ctx, feedback->message);
        else if (released && feedback->accepted)
            contentBrowserUI.reportError(ctx, {});
    }

    if (mouse.released(MouseButton::Left))
        clearAssetDrag(engine);
}
} // namespace Cthulhu::Editor