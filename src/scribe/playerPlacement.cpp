#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "components.hpp"
#include "engine.hpp"
#include "session.hpp"

namespace Cthulhu::Scribe
{
namespace
{
constexpr float eyeBelowTop = 0.15f;

std::string uniqueName(const Scene::Scene &scene, const std::string &base)
{
    if (!scene.findEntityByName(base))
        return base;

    for (std::size_t suffix = 2;; ++suffix)
    {
        std::string candidate = base + std::to_string(suffix);
        if (!scene.findEntityByName(candidate))
            return candidate;
    }
}

class PlacePlayerCommand final : public Command
{
  public:
    PlacePlayerCommand(Scene::EntityId playerId, Scene::EntityId cameraId, std::string playerName, std::string cameraName,
                       glm::vec3 position, float yaw)
        : playerId(playerId),
          cameraId(cameraId),
          playerName(std::move(playerName)),
          cameraName(std::move(cameraName)),
          position(position),
          yaw(yaw)
    {}

    std::string_view name() const override
    {
        return "Create Player";
    }

    Result apply(Context &context) override
    {
        auto &scene = context.scene;

        auto player = scene.createEntityWithId(playerId, playerName);
        if (!player)
            return Result::failed("FAILED TO CREATE PLAYER ENTITY");

        auto camera = scene.createEntityWithId(cameraId, cameraName);
        if (!camera || !scene.setParent(cameraId, playerId))
        {
            if (camera)
                scene.destroyEntity(cameraId);
            scene.destroyEntity(playerId);
            return Result::failed("FAILED TO CREATE PLAYER CAMERA");
        }

        const Scene::CharacterControllerComponent controller{};
        player->set(Scene::TransformComponent{.position = position, .rotation = {0.0f, yaw, 0.0f}});
        player->set(controller);
        player->add<Scene::TagPlayer>();

        // Transform position is the capsule's feet the eye sits just below its top.
        const float eyeHeight = controller.capsuleHeight + 2.0f * controller.capsuleRadius - eyeBelowTop;
        camera->set(Scene::TransformComponent{.position = {0.0f, eyeHeight, 0.0f}});
        const auto isPlayerCamera = [&scene](Scene::EntityId id) {
            const auto parent = scene.getParent(id);
            const auto owner = parent ? scene.findEntity(*parent) : std::nullopt;
            return owner && owner->has<Scene::TagPlayer>();
        };

        bool playerCameraIsMain = false;
        scene.getWorld().each([&](flecs::entity entity, const Scene::CameraComponent &view) {
            const auto *identity = entity.try_get<Scene::EntityIdentityComponent>();
            if (identity && view.primary && isPlayerCamera(identity->id))
                playerCameraIsMain = true;
        });

        camera->set(Scene::CameraComponent{.front = {-std::sin(yaw), 0.0f, -std::cos(yaw)}, .primary = !playerCameraIsMain});

        context.emit(ChangeType::EntityCreated, playerId);
        context.emit(ChangeType::EntityCreated, cameraId);

        // Make the new camera primary unless a player camera is already main.
        demoted.clear();
        if (!playerCameraIsMain)
            scene.getWorld().each([&](flecs::entity entity, Scene::CameraComponent &view) {
                const auto *identity = entity.try_get<Scene::EntityIdentityComponent>();
                if (!identity || identity->id == cameraId || !view.primary)
                    return;

                view.primary = false;
                demoted.push_back(identity->id);
            });

        for (const auto id : demoted)
            context.emit(ChangeType::ComponentChanged, id, "Camera");

        return Result::applied();
    }

    Result revert(Context &context) override
    {
        auto &scene = context.scene;

        for (const auto id : demoted)
        {
            if (const auto entity = scene.findEntity(id); entity && entity->has<Scene::CameraComponent>())
            {
                entity->get_mut<Scene::CameraComponent>().primary = true;
                context.emit(ChangeType::ComponentChanged, id, "Camera");
            }
        }

        const bool removedCamera = scene.destroyEntity(cameraId);
        const bool removedPlayer = scene.destroyEntity(playerId);
        if (!removedCamera || !removedPlayer)
            return Result::failed("PLAYER ENTITIES DO NOT EXIST");

        context.emit(ChangeType::EntityDestroyed, cameraId);
        context.emit(ChangeType::EntityDestroyed, playerId);
        return Result::applied();
    }

  private:
    Scene::EntityId playerId;
    Scene::EntityId cameraId;
    std::string playerName;
    std::string cameraName;
    glm::vec3 position;
    float yaw;
    std::vector<Scene::EntityId> demoted;
};
} // namespace

EntityResult Session::placePlayer(glm::vec3 position, float yaw)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(yaw))
        return {Result::failed("INVALID PLAYER PLACEMENT"), {}};

    auto *scene = syncScene();
    if (!scene)
        return {Result::failed("NO ACTIVE SCENE"), {}};

    const auto freshId = [scene](Scene::EntityId avoid) {
        auto id = Scene::generateEntityId();
        while (scene->isEntityAlive(id) || id == avoid)
            id = Scene::generateEntityId();
        return id;
    };

    const auto playerId = freshId({});
    const auto cameraId = freshId(playerId);

    Result result = execute(std::make_unique<PlacePlayerCommand>(playerId, cameraId, uniqueName(*scene, "Player"),
                                                                 uniqueName(*scene, "Camera"), position, yaw));
    return {result, result.status == Result::Status::Applied ? playerId : Scene::EntityId{}};
}
} // namespace Cthulhu::Scribe