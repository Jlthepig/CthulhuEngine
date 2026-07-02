#include "characterController.h"
#include "components.h"
#include "physics.h"
#include "flecs.h"
// Jolt Includes
#include "Jolt/Jolt.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Physics
{
   void CharacterController::create(flecs::entity e, glm::vec3 startPosition, const CharacterConfig &config, PhysicsWorld &physicsWorld)
   {
        JPH::PhysicsSystem* physicsSystem = physicsWorld.getPhysicsSystem();
        if (!physicsSystem) return;

        JPH::CapsuleShapeSettings capsuleSettings(config.capsuleHeight * 0.5f, config.capsuleRadius);
        auto capsuleShape = capsuleSettings.Create();
        if (capsuleShape.HasError()) return;

        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(0.0f, config.capsuleRadius + config.capsuleHeight * 0.5f, 0.0f),
            JPH::Quat::sIdentity(),capsuleShape.Get());

        auto offsetShape = offsetSettings.Create();
        if (offsetShape.HasError()) return;

        JPH::CharacterVirtualSettings settings;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(config.maxWalkableSlope);
        settings.mMaxStrength = config.maxPushStrength;
        settings.mShape = offsetShape.Get();
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mCharacterPadding = 0.02f;
        
        JPH::CharacterVirtual* character = new JPH::CharacterVirtual(&settings, JPH::RVec3(startPosition.x, startPosition.y, startPosition.z), JPH::Quat::sIdentity(), physicsSystem);
        Scene::CharacterControllerComponent cc;
        cc.character = character;
        cc.prevPos = startPosition;
        cc.currentPos = startPosition;
        e.set(cc);
        Log::Print("Character Controller created for entity", "CharacterController", LogType::LOG_SUCCESS);
   }    

   void CharacterController::teleport(flecs::entity e, const glm::vec3& pos)
   {
        if (!e.has<Scene::CharacterControllerComponent>()) return;
        auto& cc = e.get_mut<Scene::CharacterControllerComponent>();
        if (cc.character)
        {
            cc.character->SetPosition(JPH::RVec3(pos.x, pos.y, pos.z));
        }
        cc.prevPos = pos;
        cc.currentPos = pos;
        cc.verticalVelocity = 0.0f;
   }
   
   void CharacterController::destroy(flecs::entity e)
   {
        if (e.has<Scene::CharacterControllerComponent>())
        {
            auto& cc = e.get_mut<Scene::CharacterControllerComponent>();
            if (cc.character)
            {
                delete cc.character;
                cc.character = nullptr;
            }

            e.remove<Scene::CharacterControllerComponent>();
            Log::Print("Character Controller destroyed for entity", "CharacterController", LogType::LOG_SUCCESS);
        }
   }
}