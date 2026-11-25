#pragma once

#include <cmath>
#include <filesystem>

#include "../testCommon.hpp"
#include "session.hpp"

namespace Cthulhu::Validation
{
inline void validatePlayerPlacement(Engine &engine, Results &r)
{
    constexpr std::string_view path = "res://.cthulhu/validation_player.scene";
    const auto previousMode = engine.getWorldMode();
    engine.setWorldMode(WorldMode::Edit);

    if (!engine.createEmptyScene("V_Player"))
    {
        check(r, false, "PP0 player placement scene setup");
        engine.setWorldMode(previousMode);
        return;
    }

    auto *scene = engine.getActiveScene();
    auto oldCamera = scene->createEntity("V_OldCamera");
    oldCamera.set(CS::CameraComponent{.primary = true});

    Scribe::Session session(engine);
    const glm::vec3 position{2.0f, 0.0f, -4.0f};
    const float yaw = glm::radians(90.0f);
    const auto placed = session.placePlayer(position, yaw);
    const auto events = session.takeEvents();

    const auto matches = [&]() {
        const auto player = scene->findEntity(placed.id);
        const auto children = scene->getChildren(placed.id);
        if (!player || children.size() != 1)
            return false;

        const auto camera = scene->findEntity(children.front());
        const auto *view = camera ? camera->try_get<CS::CameraComponent>() : nullptr;
        const auto &transform = player->get<CS::TransformComponent>();

        return player->has<CS::CharacterControllerComponent>() && player->has<CS::TagPlayer>() &&
               transform.position == position && std::abs(transform.rotation.y - yaw) < 1.0e-5f &&
               view && view->primary && glm::length(view->front - glm::vec3(-1.0f, 0.0f, 0.0f)) < 1.0e-4f &&
               camera->get<CS::TransformComponent>().position.y > 0.0f &&
               !oldCamera.get<CS::CameraComponent>().primary;
    };

    check(r, placed.ok() && matches() && events.size() == 3,
          "PP1 Create Player builds player, eye camera and main-camera switch in one step");

    const bool undone = session.undo();
    check(r, undone && !scene->isEntityAlive(placed.id) && oldCamera.get<CS::CameraComponent>().primary &&
                 !session.canUndo(),
          "PP2 one undo removes the player and restores the old main camera");

    const bool redone = session.redo();
    check(r, redone && matches(), "PP3 redo restores the same player");

    const bool saved = session.saveAs(path).ok();
    const bool reopened = saved && engine.loadScene(path);
    scene = engine.getActiveScene();

    bool roundTrip = false;
    if (reopened && scene)
    {
        const auto playerId = scene->findEntityByName("Player");
        const auto player = playerId ? scene->findEntity(*playerId) : std::nullopt;
        const auto children = playerId ? scene->getChildren(*playerId) : std::vector<CS::EntityId>{};
        const auto camera = children.size() == 1 ? scene->findEntity(children.front()) : std::nullopt;
        const auto *view = camera ? camera->try_get<CS::CameraComponent>() : nullptr;

        roundTrip = player && player->has<CS::CharacterControllerComponent>() && player->has<CS::TagPlayer>() &&
                    std::abs(player->get<CS::TransformComponent>().rotation.y - yaw) < 1.0e-5f && view && view->primary;
    }
    check(r, roundTrip, "PP4 player survives save and reopen");

    if (const auto *project = engine.getProject())
    {
        if (const auto file = project->resolveResourcePath(path))
        {
            std::error_code error;
            std::filesystem::remove(*file, error);
        }
    }

    engine.setWorldMode(previousMode);
    check(r, engine.loadScene(SAVE_PATH), "PP5 restore validation scene");
}
} // namespace Cthulhu::Validation