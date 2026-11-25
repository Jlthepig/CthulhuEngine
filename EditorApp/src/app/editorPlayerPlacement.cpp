#include <cmath>

#include "editorShell.hpp"

#include "camera.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "scene.hpp"
#include "session.hpp"

namespace Cthulhu::Editor
{
void EditorShell::createPlayer(Context &ctx, Engine &engine, Scribe::Session &session)
{
    auto *scene = engine.getActiveScene();
    const auto *camera = engine.getCamera();
    if (!scene || !camera || engine.isPlaying() || session.transformEditActive())
        return;

    const auto position = viewportPanelUI.spawnPosition(ctx, engine, sceneNode);
    if (!position)
    {
        contentBrowserUI.reportError(ctx, "No place to put the player. Show the scene in the viewport and try again.");
        return;
    }

    const glm::vec3 front = camera->getFront();
    const float yaw = std::abs(front.x) + std::abs(front.z) > 1.0e-4f ? std::atan2(-front.x, -front.z) : 0.0f;

    const bool hadPlayer = scene->getWorld().count<Scene::TagPlayer>() > 0;
    session.endMerge();
    const auto placed = session.placePlayer(*position, yaw);
    if (!placed.ok())
    {
        contentBrowserUI.reportError(ctx, placed.result.error);
        return;
    }

    pendingEntitySelection = {placed.id};
    pendingEntitySelectionGeneration = engine.getSceneGeneration();
    contentBrowserUI.reportError(ctx, hadPlayer ? "This scene already has a player. Only one player is used in Play." : "");
}
} // namespace Cthulhu::Editor