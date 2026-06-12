#include "pch.h"
#include "Jolt/jolt.h"
#include "physics.h"
#include "Jolt/Core/Core.h"
#include "Jolt/Core/IssueReporting.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ContactListener.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "characterController.h"
#include "gtc/quaternion.hpp"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/Memory.h"
#include "log_utils.hpp"
#include <cstdarg>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

static void joltTrace(const char* inFMT, ...)
{
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    Log::Print(buffer, "Jolt", LogType::LOG_INFO);
}

namespace Cthulhu::Physics
{
    namespace ObjectLayers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYER = 2;
    }
    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYER(2);
    }
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYER; }
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            if (layer == ObjectLayers::NON_MOVING) return BroadPhaseLayers::NON_MOVING;
            return BroadPhaseLayers::MOVING;
        }
    };

    class ObjVsBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer objectLayer, JPH::BroadPhaseLayer bpLayer) const override
        {
            if (objectLayer == ObjectLayers::NON_MOVING) return bpLayer == BroadPhaseLayers::MOVING;
            return true;
        }
    };

    class ObjLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
        {
            if (layer1 == ObjectLayers::NON_MOVING && layer2 == ObjectLayers::NON_MOVING) return false;
            return true;
        }
    };

    class ContactListenerImpl final : public JPH::ContactListener {};

    void PhysicsWorld::init(const PhysicsConfig& config)
    {        
        this->config = config;
        JPH::Trace = joltTrace;
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        
        tempAllocator = new JPH::TempAllocatorImpl(this->config.tempAllocatorSizeMB * 1024 * 1024);
        jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, this->config.threadCount);

        // Allocate the filters
        bpLayerInterface = new BPLayerInterfaceImpl();
        objVsBpFilter = new ObjVsBPLayerFilterImpl();
        objLayerPairFilter = new ObjLayerPairFilterImpl();
        contactListener = new ContactListenerImpl();

        physicsSystem = new JPH::PhysicsSystem();
        physicsSystem->Init(
            this->config.maxBodies,
            0,
            this->config.maxBodyPairs, 
            this->config.maxContactConstraints,
            *bpLayerInterface,
            *objVsBpFilter,
            *objLayerPairFilter
        );

        physicsSystem->SetContactListener(contactListener);
        Log::Print("Initialized Jolt Physics System", "Physics", LogType::LOG_INFO);
    }

    void PhysicsWorld::step(float deltaTime)
    {
        if (!physicsSystem) return;
        physicsAccumulator += deltaTime;
        if (physicsAccumulator > this->config.maxDeltaTime)
            physicsAccumulator = this->config.maxDeltaTime;

        while (physicsAccumulator >= this->config.fixedDeltaTime)    
        {
            CharacterController::fixedUpdate(this->config.fixedDeltaTime, *this); 
            physicsSystem->Update(
                this->config.fixedDeltaTime,
                this->config.collisionSteps,
                tempAllocator,
                jobSystem
            );
            physicsAccumulator -= this->config.fixedDeltaTime;
        }
    }

    uint32_t PhysicsWorld::addStaticBox(glm::vec3 position, glm::vec3 halfExtent)
    {
        if (!physicsSystem) { /* error */ return 0; }
        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
        auto shapeResult = shapeSettings.Create();
        if (shapeResult.HasError()) { /* error */ return 0; }

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(), JPH::RVec3(position.x, position.y, position.z),
            JPH::Quat::sIdentity(), JPH::EMotionType::Static, ObjectLayers::NON_MOVING);

        JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
        return bodyId.GetIndexAndSequenceNumber();
    }

    uint32_t PhysicsWorld::addDynamicBox(glm::vec3 position, glm::vec3 halfExtent, float mass)
    {
        if (!physicsSystem) { /* error */ return 0; }
        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
        auto shapeResult = shapeSettings.Create();
        if (shapeResult.HasError()) { /* error */ return 0; }

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(), JPH::RVec3(position.x, position.y, position.z),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, ObjectLayers::MOVING);

        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bodySettings.mMassPropertiesOverride.mMass = mass;

        JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::Activate);
        return bodyId.GetIndexAndSequenceNumber();
    }

    BodyTransform PhysicsWorld::getBodyTransform(uint32_t bodyIdValue)
    {
        BodyTransform result;
        if (!physicsSystem) return result;

        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        JPH::BodyID bodyId(bodyIdValue);

        JPH::RVec3 pos = bodyInterface.GetPosition(bodyId);
        JPH::Quat rot = bodyInterface.GetRotation(bodyId);

        result.position = glm::vec3(static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()));
        glm::quat glmRot(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
        result.rotation = glm::eulerAngles(glmRot);

        return result;
    }

    void PhysicsWorld::createGroundPlane()
    {
        JPH::BodyInterface &bodyInterface = physicsSystem->GetBodyInterface();
        JPH::BoxShapeSettings groundShapeSettings(JPH::Vec3(this->config.groundWidth, this->config.groundHeight, this->config.groundDepth));
        auto groundShape = groundShapeSettings.Create();
        if (groundShape.HasError()) { /* error */ return; }

        JPH::BodyCreationSettings groundSettings(
            groundShape.Get(), JPH::Vec3(0.0f, -this->config.groundHeight, 0.0f),
            JPH::Quat::sIdentity(), JPH::EMotionType::Static, ObjectLayers::NON_MOVING);
        groundSettings.mFriction = this->config.groundFriction;
        bodyInterface.CreateAndAddBody(groundSettings, JPH::EActivation::DontActivate);
    }

    float PhysicsWorld::getInterpolationAlpha()
    {
        return physicsAccumulator / this->config.fixedDeltaTime;
    }

    void PhysicsWorld::shutdown()
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        delete physicsSystem;
        delete jobSystem;
        delete tempAllocator;

        delete bpLayerInterface;
        delete objVsBpFilter;
        delete objLayerPairFilter;
        delete contactListener;

        physicsSystem = nullptr;
        jobSystem = nullptr;
        tempAllocator = nullptr;
    }
}