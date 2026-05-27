#include "characterController.h"

#include "Jolt/Jolt.h"
#include "Jolt/Math/Quat.h"
#include "Jolt/Math/Real.h"
#include "Jolt/Math/Vec3.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Core/TempAllocator.h"
#include "fwd.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace JPH { class PhysicsSystem; }

namespace
{
    JPH::CharacterVirtual* character = nullptr;
    static Cthulhu::Physics::CharacterConfig s_CharacterConfig;

    // Current vertical velocity (gravity accumulates here)
    float verticalVelocity = 0.0f;

    glm::vec3 prevPos(0.0f);
    glm::vec3 pendingMove(0.0f);
    bool pendingJump = false;

}
namespace Cthulhu::Physics
{
    void CharacterController::init(glm::vec3 startPosition, const CharacterConfig& config, PhysicsWorld& physicsWorld)
    {
         s_CharacterConfig = config;

        JPH::PhysicsSystem* physicsSystem = physicsWorld.getPhysicsSystem();
        if (!physicsSystem)
        {
            Log::Print("CANNOT CREATE CHARACTER CONTROLLER - PHYSICS NOT INITIALIZED", "CharacterController", LogType::LOG_ERROR);
            return;
        }

        // capsule shape default for all fps games
        // half height is half the cylinder part not including the hemispheres
        JPH::CapsuleShapeSettings capsuleSettings(s_CharacterConfig.capsuleHeight * 0.5f, s_CharacterConfig.capsuleRadius);
        auto capsuleShape = capsuleSettings.Create();

        if (capsuleShape.HasError())
        {
            Log::Print("FAILED TO CREATE CAPSULE SHAPE", "CharacterController", LogType::LOG_ERROR);
            return;
        }

        // offset the shape so feet are at y=0 not the center or character sinks into ground
        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(0.0f,s_CharacterConfig.capsuleRadius + s_CharacterConfig.capsuleHeight * 0.5f,0.0f), 
            JPH::Quat::sIdentity(),
            capsuleShape.Get()
        );

        auto offsetShape = offsetSettings.Create();

        if (offsetShape.HasError())
        {
            Log::Print("FAILED TO CREATE OFFSET SHAPE", "CharacterController", LogType::LOG_ERROR);
            return;
        }

        // character settings
        JPH::CharacterVirtualSettings settings;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(s_CharacterConfig.maxWalkableSlope);
        settings.mMaxStrength = s_CharacterConfig.maxPushStrength;                  // how hard it can push
        settings.mShape = offsetShape.Get();
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mCharacterPadding = 0.02f;
       
        // create the character controller
        character = new JPH::CharacterVirtual(
            &settings,
            JPH::RVec3(startPosition.x, startPosition.y, startPosition.z),
            JPH::Quat::sIdentity(),
            physicsSystem
        );

        prevPos = startPosition;
        Log::Print("Character Controller Initialized", "CharacterController", LogType::LOG_SUCCESS);
    }

    // This is the new method for queuing input. It simply stores the latest movement and jump input, which will be processed in fixedUpdate for consistent physics behavior.
    void CharacterController::queueInput(glm::vec3 movement, bool jump)
    {
        pendingMove = movement;
        pendingJump = pendingJump || jump;
    }

    // This is the new fixed update method that processes queued input for consistent physics behavior.
    void CharacterController::fixedUpdate(float fixedDt, PhysicsWorld& physicsWorld)
    {
        if (!character) return;

        prevPos = getPosition();

        auto groundState = character->GetGroundState();
        bool isOnGround = groundState == JPH::CharacterBase::EGroundState::OnGround;

        if (isOnGround) {
            verticalVelocity = 0.0f;
            if (pendingJump) verticalVelocity = s_CharacterConfig.jumpVelocity;
        } else {
            verticalVelocity += s_CharacterConfig.gravity * fixedDt;
        }

        JPH::Vec3 velocity(pendingMove.x, verticalVelocity, pendingMove.z);
        character->SetLinearVelocity(velocity);

        JPH::CharacterVirtual::ExtendedUpdateSettings s;
        character->ExtendedUpdate(
            fixedDt,
            JPH::Vec3(0,0,0),
            s,
            physicsWorld.getPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(0),
            physicsWorld.getPhysicsSystem()->GetDefaultLayerFilter(0),
            {}, {}, *physicsWorld.getTempAllocator()
        );

        pendingJump = false; // consume
    }
    
    glm::vec3 CharacterController::getPosition()
    {
        if (!character) return glm::vec3(0.0f);

        JPH::RVec3 pos = character->GetPosition();
        return glm::vec3(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ())
        );
    }

    glm::vec3 CharacterController::getInterpolationPosition(float alpha)
    {
        if (!character) return glm::vec3(0.0f);

        glm::vec3 currentPos = getPosition();
        return glm::mix(prevPos, currentPos, alpha);
    }

    void CharacterController::destroy()
    {
        delete character;
        character = nullptr;
        verticalVelocity = 0.0;
        Log::Print("Character controller destroyed", "CharacterController", LogType::LOG_INFO);
    }
}