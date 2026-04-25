#include "physics.h"

#include "Jolt/Core/Core.h"
#include "Jolt/Core/IssueReporting.h"
#include "Jolt/Jolt.h"
#include "Jolt/Physics/Collision/ContactListener.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "gtc/quaternion.hpp"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/Memory.h"
#include "log_utils.hpp"
#include <cstdarg>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// LAYER SYSTEM 
// JOLT uses a layer system to determine which objects should collide with each other.
// We need to define our own layers and tell JOLT about them.
// I chose 2 layers one for moving objects and one for non moving

namespace
{
    // Physics system configuration
    constexpr int TEMP_ALLOCATOR_SIZE_MB = 10;
    constexpr int PHYSICS_THREAD_COUNT = 1;
    constexpr int MAX_BODIES = 1024;
    constexpr int MAX_BODY_PAIRS = 1024;
    constexpr int MAX_CONTACT_CONSTRAINTS = 1024;
    constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
    constexpr int COLLISION_STEPS = 1;
    constexpr size_t TRACE_BUFFER_SIZE = 1024;

    // Ground plane configuration
    constexpr float GROUND_PLANE_WIDTH = 100.0f;
    constexpr float GROUND_PLANE_HEIGHT = 0.5f;
    constexpr float GROUND_PLANE_DEPTH = 100.0f;
    namespace ObjectLayers
    {
        // assigned to each object or body
        static constexpr JPH::ObjectLayer MOVING = 0;
        static constexpr JPH::ObjectLayer NON_MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYER = 2;
    }

    namespace BroadPhaseLayers
    {
        // used internally by JOLT to speed up collision checks
        static constexpr JPH::BroadPhaseLayer MOVING(0);
        static constexpr JPH::BroadPhaseLayer NON_MOVING(1);
        static constexpr JPH::uint NUM_LAYER(2);
    }

    // Mapping from object layers to broad phase layers
    // tell jolt Non moving objects go to non moving bucket same for moving objects
    class BroadPhaseLayerInterface : public JPH::BroadPhaseLayerInterface
    {
        public:
            virtual JPH::uint GetNumBroadPhaseLayers() const override
            {
                return BroadPhaseLayers::NUM_LAYER;
            }

            virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
            {
                if (layer == ObjectLayers::NON_MOVING)
                    return BroadPhaseLayers::NON_MOVING;
                return BroadPhaseLayers::MOVING;
            }

    };

    // can this object layer collide with this broad phase layer
    // no need to check non moving objects against each other waste of time

    class ObjectVsBroadPhaseLayerFilter : public JPH::ObjectVsBroadPhaseLayerFilter
    {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer objectLayer, JPH::BroadPhaseLayer bpLayer) const override
            {
                if (objectLayer == ObjectLayers::NON_MOVING)
                    return bpLayer == BroadPhaseLayers::MOVING;
                return true;
            }
    };

    // can these 2 object layers collide with each other
    class ObjectLayerPairFilter : public JPH::ObjectLayerPairFilter
    {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
            {
                if (layer1 == ObjectLayers::NON_MOVING && layer2 == ObjectLayers::NON_MOVING)
                    return false;
                return true;
            }
    };

    // empty contact listenr i'll do it later not needed now
    class ContactListener : public JPH::ContactListener
    {

    };

    // Static state lives for entire engine lifetime

    BroadPhaseLayerInterface bpLayerInterface;
    ObjectVsBroadPhaseLayerFilter objVsBpFilter;
    ObjectLayerPairFilter objLayerPairFilter;
    ContactListener contactListener;

    JPH::TempAllocatorImpl* tempAllocator = nullptr;
    JPH::JobSystemThreadPool* jobSystem = nullptr;
    JPH::PhysicsSystem* physicsSystem = nullptr;
}

// trace callback jolt uses this instead of printf
static void joltTrace(const char* inFMT, ...)
{
    va_list list;
    va_start(list, inFMT);
    char buffer[TRACE_BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    Log::Print(buffer, "Jolt", LogType::LOG_INFO);
}

JPH::PhysicsSystem* getPhysicsSystem() { return physicsSystem; }
JPH::TempAllocatorImpl* getTempAllocator() { return tempAllocator; }

// init jolt
namespace Cthulhu::Physics
{
    void Physics::init()
    {
        // tell jolt how to log and assert
        
    JPH::Trace = joltTrace;
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    Log::Print("Jolt types registered successfully", "Physics", LogType::LOG_SUCCESS);
        // temp allocation jolt needs a little bit of memory for calculations
        // 10mb should be good
        tempAllocator = new JPH::TempAllocatorImpl(TEMP_ALLOCATOR_SIZE_MB * 1024 * 1024);
        // job system jolt is able to use multiple threads but for now we'll just use
        // 1 physics thread + main thread
        jobSystem = new JPH::JobSystemThreadPool(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            PHYSICS_THREAD_COUNT);

        // create the physics system this is the world
        physicsSystem = new JPH::PhysicsSystem();
        physicsSystem->Init(
        MAX_BODIES,
        0,
        MAX_BODY_PAIRS, 
        MAX_CONTACT_CONSTRAINTS,
        bpLayerInterface,
        objVsBpFilter,
        objLayerPairFilter
            );

        physicsSystem->SetContactListener(&contactListener);
        Log::Print("Initialized Jolt Physics System", "Physics", LogType::LOG_INFO);
    }

    void Physics::step(float deltaTime)
    {
        if (!physicsSystem) return;

        // jolt wants fixed timesteps for stabilty
        // 1/60th of a second is a good default

        constexpr float fixedDeltaTime = FIXED_TIMESTEP;
        physicsSystem->Update(
            fixedDeltaTime,
            COLLISION_STEPS,  // collision steps per update
            tempAllocator,
            jobSystem
        );
    }

    uint32_t Physics::addStaticBox(glm::vec3 position, glm::vec3 halfExtent)
    {
        if (!physicsSystem)
        {
            Log::Print("Cannot create static box - physics system not initialized", "Physics", LogType::LOG_ERROR);
            return 0;
        }

        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

        JPH::BoxShapeSettings shapeSettings(
        JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z)
        );

        auto shapeResult = shapeSettings.Create();
        if (shapeResult.HasError())
        {
            Log::Print("Failed to create static box shape", "Physics", LogType::LOG_ERROR);
            return 0;
        }

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(),
            JPH::RVec3(position.x, position.y, position.z),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            ObjectLayers::NON_MOVING
        );

        JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(
            bodySettings,
            JPH::EActivation::DontActivate
        );

        Log::Print("Created static box body", "Physics", LogType::LOG_INFO);

        return bodyId.GetIndexAndSequenceNumber();
    }

    uint32_t Physics::addDynamicBox(glm::vec3 position, glm::vec3 halfExtent, float mass)
    {
        if (!physicsSystem)
        {
            Log::Print("Cannot create dynamic box - physics system not initialized", "Physics", LogType::LOG_ERROR);
            return 0;
        }

        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

        JPH::BoxShapeSettings shapeSettings(
            JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z)
        );

        auto shapeResult = shapeSettings.Create();

        if (shapeResult.HasError())
        {
            Log::Print("Failed to create dynamic box shape", "Physics", LogType::LOG_ERROR);
            return 0;
        }

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(),
            JPH::RVec3(position.x, position.y, position.z),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            ObjectLayers::MOVING
        );

        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bodySettings.mMassPropertiesOverride.mMass = mass;

        JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(
            bodySettings,
            JPH::EActivation::Activate
        );

        Log::Print("Created dynamic box body", "Physics", LogType::LOG_INFO);

        return bodyId.GetIndexAndSequenceNumber();
    }

    BodyTransform Physics::getBodyTransform(uint32_t bodyIdValue)
    {
        BodyTransform result;

        if (!physicsSystem)
            return result;

        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

        JPH::BodyID bodyId(bodyIdValue);

        JPH::RVec3 pos = bodyInterface.GetPosition(bodyId);
        JPH::Quat rot = bodyInterface.GetRotation(bodyId);

        result.position = glm::vec3(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ())
        );

        glm::quat glmRot(
            rot.GetW(),
            rot.GetX(),
            rot.GetY(),
            rot.GetZ()
        );

        result.rotation = glm::eulerAngles(glmRot);

        return result;
    
    }


    void Physics::createGroundPlane()
    {
        // we need body interface to create bodies in the world
        JPH::BodyInterface &bodyInterface = physicsSystem->GetBodyInterface();

        // create a large thin box to act as the ground plane at y = 0
        JPH::BoxShapeSettings groundShapeSettings(JPH::Vec3(GROUND_PLANE_WIDTH, GROUND_PLANE_HEIGHT, GROUND_PLANE_DEPTH));
        auto groundShape = groundShapeSettings.Create();

        if (groundShape.HasError())
        {
            Log::Print("FAILED TO CREATE GROUND SHAPE", "Physics", LogType::LOG_ERROR);
            return;
        }

        // boddy creation settins for pos rot shape layer and motion type
        JPH::BodyCreationSettings groundSettings(
            groundShape.Get(), // shape
            JPH::Vec3(0.0f, -GROUND_PLANE_HEIGHT, 0.0f), // push it down half its height so top face sits at Y=0
            JPH::Quat::sIdentity(), // no rotation
            JPH::EMotionType::Static, // motion type never moves
            ObjectLayers::NON_MOVING // coll layer
        );

        bodyInterface.CreateAndAddBody(groundSettings, JPH::EActivation::DontActivate);

        Log::Print("Ground plane created", "Physics", LogType::LOG_SUCCESS);
    }

    void Physics::shutdown()
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        delete physicsSystem;
        delete jobSystem;
        delete tempAllocator;

        physicsSystem = nullptr;
        jobSystem = nullptr;
        tempAllocator = nullptr;

        Log::Print("Shutdown Jolt Physics System", "Physics", LogType::LOG_INFO);
    }
}