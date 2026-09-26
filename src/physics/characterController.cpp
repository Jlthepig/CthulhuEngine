#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <flecs.h>

#include "physics.hpp"
#include "characterController.hpp"
#include "components.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Physics
{

bool CharacterController::createRuntime(flecs::entity entity, PhysicsWorld &physicsWorld)
{
    if (!entity.has<Scene::CharacterControllerComponent>() || !entity.has<Scene::TransformComponent>())
    {
        return false;
    }

    JPH::PhysicsSystem* physicsSystem = physicsWorld.getPhysicsSystem();
    if (!physicsSystem)
    {
        return false;
    }

    const auto& config = entity.get<Scene::CharacterControllerComponent>();
    const auto& transform = entity.get<Scene::TransformComponent>();

    JPH::CapsuleShapeSettings capsuleSettings(config.capsuleHeight * 0.5f, config.capsuleRadius);

    auto capsuleShape = capsuleSettings.Create();
    if (capsuleShape.HasError())
    {
        return false;
    }

    JPH::RotatedTranslatedShapeSettings offsetSettings(JPH::Vec3(0.0f, config.capsuleRadius + config.capsuleHeight * 0.5f, 0.0f), 
    JPH::Quat::sIdentity(), capsuleShape.Get());

    auto offsetShape = offsetSettings.Create();
    if (offsetShape.HasError())
    {
        return false;
    }

    JPH::CharacterVirtualSettings settings;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(config.maxWalkableSlope);
    settings.mMaxStrength = config.maxPushStrength;
    settings.mShape = offsetShape.Get();
    settings.mUp = JPH::Vec3::sAxisY();
    settings.mCharacterPadding = 0.02f;

    auto* character = new JPH::CharacterVirtual(&settings, JPH::RVec3(transform.position.x, transform.position.y, transform.position.z), 
    JPH::Quat::sIdentity(), physicsSystem);

    if (entity.has<Scene::CharacterControllerRuntimeComponent>())
    {
        auto& oldRuntime = entity.get_mut<Scene::CharacterControllerRuntimeComponent>();

        delete oldRuntime.character;
        oldRuntime.character = nullptr;
    }

    Scene::CharacterControllerRuntimeComponent runtime;
    runtime.character = character;
    runtime.prevPos = transform.position;
    runtime.currentPos = transform.position;

    entity.set(runtime);
    return true;
}

void CharacterController::teleport(flecs::entity entity, const glm::vec3 &position)
{
    if (!entity.has<Scene::CharacterControllerRuntimeComponent>())
    {
        return;
    }

    auto& runtime = entity.get_mut<Scene::CharacterControllerRuntimeComponent>();

    if(runtime.character)
    {
        runtime.character->SetPosition(JPH::RVec3(position.x,position.y,position.z));
    }
    runtime.prevPos = position;
    runtime.currentPos = position;
    runtime.verticalVelocity = 0.0f;
}

void CharacterController::destroyRuntime(flecs::entity entity)
{
    if (entity.has<Scene::CharacterControllerRuntimeComponent>())
    {
        entity.remove<Scene::CharacterControllerRuntimeComponent>();
    }
}

} // namespace Cthulhu::Physics