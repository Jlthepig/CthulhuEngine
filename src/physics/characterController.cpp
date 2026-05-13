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
extern JPH::PhysicsSystem* getPhysicsSystem();
extern JPH::TempAllocatorImpl* getTempAllocator();
namespace
{
    JPH::CharacterVirtual* character = nullptr;

    // Physics constants
    constexpr float gravity = -9.81f;
    constexpr float jumpVelocity = 5.0f;

    // Player dimensions
    constexpr float capsuleRadius = 0.3f;
    constexpr float capsuleHeight = 2.0f;

    // Character controller settings
    constexpr float maxWalkableSlope = 45.0f;  // degrees
    constexpr float maxPushStrength = 100.0f;

    // Current vertical velocity (gravity accumulates here)
    float verticalVelocity = 0.0f;

    glm::vec3 pendingMove(0.0f);
    bool pendingJump = false;

}
namespace Cthulhu::Physics
{
    void CharacterController::init(glm::vec3 startPosition)
    {
        JPH::PhysicsSystem* physicsSystem = getPhysicsSystem();
        if (!physicsSystem)
        {
            Log::Print("CANNOT CREATE CHARACTER CONTROLLER - PHYSICS NOT INITIALIZED", "CharacterController", LogType::LOG_ERROR);
            return;
        }

        // capsule shape default for all fps games
        // half height is half the cylinder part not including the hemispheres
        JPH::CapsuleShapeSettings capsuleSettings(capsuleHeight * 0.5f, capsuleRadius);
        auto capsuleShape = capsuleSettings.Create();

        if (capsuleShape.HasError())
        {
            Log::Print("FAILED TO CREATE CAPSULE SHAPE", "CharacterController", LogType::LOG_ERROR);
            return;
        }

        // offset the shape so feet are at y=0 not the center or character sinks into ground
        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(0.0f,capsuleRadius + capsuleHeight * 0.5f,0.0f), 
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
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(maxWalkableSlope);
        settings.mMaxStrength = maxPushStrength;                  // how hard it can push
        settings.mShape = offsetShape.Get();
        settings.mUp = JPH::Vec3::sAxisY();                     // Y is up
       
        // create the character controller
        character = new JPH::CharacterVirtual(
            &settings,
            JPH::RVec3(startPosition.x, startPosition.y, startPosition.z),
            JPH::Quat::sIdentity(),
            physicsSystem
        );

        Log::Print("Character Controller Initialized", "CharacterController", LogType::LOG_SUCCESS);
    }

    // This is the new method for queuing input. It simply stores the latest movement and jump input, which will be processed in fixedUpdate for consistent physics behavior.
    void CharacterController::queueInput(glm::vec3 movement, bool jump)
    {
        pendingMove = movement;
        pendingJump = pendingJump || jump;
    }

    // This is the new fixed update method that processes queued input for consistent physics behavior.
    void CharacterController::fixedUpdate(float fixedDt)
    {
        if (!character) return;

        auto groundState = character->GetGroundState();
        bool isOnGround = groundState == JPH::CharacterBase::EGroundState::OnGround;

        if (isOnGround) {
            verticalVelocity = 0.0f;
            if (pendingJump) verticalVelocity = jumpVelocity;
        } else {
            verticalVelocity += gravity * fixedDt;
        }

        JPH::Vec3 velocity(pendingMove.x, verticalVelocity, pendingMove.z);
        character->SetLinearVelocity(velocity);

        JPH::CharacterVirtual::ExtendedUpdateSettings s;
        character->ExtendedUpdate(
            fixedDt,
            JPH::Vec3(0,0,0),
            s,
            getPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(0),
            getPhysicsSystem()->GetDefaultLayerFilter(0),
            {}, {}, *getTempAllocator()
        );

        pendingJump = false; // consume
    }
    
    // This is the old update method that directly applies movement and jump input. It's now deprecated in favor of queueing input and processing it in fixedUpdate for better physics consistency.
    void CharacterController::update(glm::vec3 movementInput, bool jump, float deltaTime)
    {
        if (!character) return;

        // Check ground state
        auto groundState = character->GetGroundState();
        bool isOnGround = groundState == JPH::CharacterBase::EGroundState::OnGround;

        // When on ground, reset vertical and apply jump
        if (isOnGround)
        {
            verticalVelocity = 0.0f;
            if (jump)
            {
                verticalVelocity = jumpVelocity;
            }
        }
        else
        {
            // In air: apply gravity to accumulated vertical velocity
            verticalVelocity += gravity * deltaTime;
        }
        // Set velocity: horizontal from input, vertical from our accumulated gravity
        JPH::Vec3 velocity(movementInput.x, verticalVelocity, movementInput.z);
        character->SetLinearVelocity(velocity);

        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
        character->ExtendedUpdate(
            deltaTime,
            JPH::Vec3(0.0f, 0.0f, 0.0f),
            updateSettings,
            getPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(0),
            getPhysicsSystem()->GetDefaultLayerFilter(0),
            {},
            {},
            *getTempAllocator()
        );
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

    void CharacterController::destroy()
    {
        delete character;
        character = nullptr;
        verticalVelocity = 0.0;
        Log::Print("Character controller destroyed", "CharacterController", LogType::LOG_INFO);
    }
}