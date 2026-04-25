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

    void CharacterController::update(glm::vec3 movementInput, bool jump, float deltaTime)
    {
        if (!character) return;

        // apply gravity
        if (character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround)
        {
            verticalVelocity = 0.0f; // reset vertical velocity when on ground
            if (jump)
            {
                verticalVelocity = jumpVelocity; // apply jump velocity
            }
        }
        else
        {
            verticalVelocity += gravity * deltaTime; // accumulate gravity
        }

        // combine horizontal input with vertical velocity
        JPH::Vec3 velocity(movementInput.x, verticalVelocity, movementInput.z);
        character->SetLinearVelocity(velocity);

        // update settins for this step
        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
        character->ExtendedUpdate(
            deltaTime,
            JPH::Vec3(0.0f, gravity, 0.0f), // gravity vector
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