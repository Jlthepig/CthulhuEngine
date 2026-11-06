#include <cmath>
#include <cstdarg>
#include <limits>
#include <algorithm>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Constraints/ContactConstraintManager.h>

#include <gtc/quaternion.hpp>

#include "characterController.hpp"
#include "log_utils.hpp"
#include "physics.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

static void joltTrace(const char *inFMT, ...)
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
} // namespace ObjectLayers

namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr JPH::uint NUM_LAYER(2);
} // namespace BroadPhaseLayers

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
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

class ObjVsBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
  public:
    virtual bool ShouldCollide(JPH::ObjectLayer objectLayer, JPH::BroadPhaseLayer bpLayer) const override
    {
        if (objectLayer == ObjectLayers::NON_MOVING)
            return bpLayer == BroadPhaseLayers::MOVING;
        return true;
    }
};

class ObjLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
  public:
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        if (layer1 == ObjectLayers::NON_MOVING && layer2 == ObjectLayers::NON_MOVING)
            return false;
        return true;
    }
};

class ContactListenerImpl final : public JPH::ContactListener
{};

PhysicsWorld::~PhysicsWorld()
{
    shutdown();
}

bool PhysicsWorld::init(const PhysicsConfig &config)
{
    constexpr std::size_t bytesPerMB = 1024u * 1024u;

    const bool validConfig =
        config.tempAllocatorSizeMB > 0 &&
        static_cast<std::size_t>(config.tempAllocatorSizeMB) <=
            std::numeric_limits<std::size_t>::max() / bytesPerMB &&
        config.threadCount >= -1 &&
        config.maxBodies > 0 &&
        static_cast<std::uint32_t>(config.maxBodies) <=
            JPH::BodyID::cMaxBodyIndex + 1u &&
        config.maxBodyPairs > 0 &&
        static_cast<std::uint32_t>(config.maxBodyPairs) <=
            JPH::ContactConstraintManager::cMaxBodyPairsLimit &&
        config.maxContactConstraints > 0 &&
        static_cast<std::uint32_t>(config.maxContactConstraints) <=
            JPH::ContactConstraintManager::cMaxContactConstraintsLimit &&
        config.collisionSteps > 0 &&
        config.maxStepsPerUpdate > 0 &&
        std::isfinite(config.fixedDeltaTime) &&
        config.fixedDeltaTime > 0.0f &&
        std::isfinite(1.0f / config.fixedDeltaTime) &&
        std::isfinite(config.maxDeltaTime) &&
        config.maxDeltaTime >= config.fixedDeltaTime &&
        std::isfinite(config.groundWidth) && config.groundWidth > 0.0f &&
        std::isfinite(config.groundHeight) && config.groundHeight > 0.0f &&
        std::isfinite(config.groundDepth) && config.groundDepth > 0.0f &&
        std::isfinite(config.groundFriction) && config.groundFriction >= 0.0f;

    if (!validConfig)
    {
        Log::Print("INVALID PHYSICS CONFIGURATION", "Physics", LogType::LOG_ERROR);
        return false;
    }

    if (physicsSystem || factory || JPH::Factory::sInstance)
    {
        Log::Print("JOLT ALREADY HAS AN OWNER", "Physics", LogType::LOG_ERROR);
        return false;
    }

    this->config = config;
    JPH::Trace = joltTrace;
    JPH::RegisterDefaultAllocator();
    factory = new JPH::Factory();
    JPH::Factory::sInstance = factory;
    JPH::RegisterTypes();

    const std::size_t allocatorBytes = static_cast<std::size_t>(config.tempAllocatorSizeMB) * bytesPerMB;

    tempAllocator = new JPH::TempAllocatorImpl(allocatorBytes);
    jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, this->config.threadCount);

    // Allocate the filters
    bpLayerInterface = new BPLayerInterfaceImpl();
    objVsBpFilter = new ObjVsBPLayerFilterImpl();
    objLayerPairFilter = new ObjLayerPairFilterImpl();
    contactListener = new ContactListenerImpl();

    physicsSystem = new JPH::PhysicsSystem();
    physicsSystem->Init(this->config.maxBodies,
                        0,
                        this->config.maxBodyPairs,
                        this->config.maxContactConstraints,
                        *bpLayerInterface,
                        *objVsBpFilter,
                        *objLayerPairFilter);

    physicsSystem->SetContactListener(contactListener);
    Log::Print("Initialized Jolt Physics System", "Physics", LogType::LOG_SUCCESS);
    physicsAccumulator = 0.0;
    return true;
}

void PhysicsWorld::step(float deltaTime)
{
    if (!physicsSystem || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    const double fixedDt = config.fixedDeltaTime;
    physicsAccumulator = std::min(physicsAccumulator + static_cast<double>(deltaTime), static_cast<double>(config.maxDeltaTime));

    int steps{};

    while (physicsAccumulator >= fixedDt && steps < config.maxStepsPerUpdate)
    {
        if (onFixedUpdate) onFixedUpdate(onFixedUpdateContext, config.fixedDeltaTime);

        physicsSystem->Update(config.fixedDeltaTime, config.collisionSteps, tempAllocator, jobSystem);

        physicsAccumulator -= fixedDt;
        ++steps;
    }

    if (physicsAccumulator >= fixedDt)
    {
        physicsAccumulator = std::fmod(physicsAccumulator, fixedDt);
    }
}

std::optional<uint32_t> PhysicsWorld::addStaticBox(glm::vec3 position, glm::quat rotation, glm::vec3 halfExtent)
{
    if (!physicsSystem)
    { /* error */
        return std::nullopt;
    }

    if (!std::isfinite(halfExtent.x) || halfExtent.x <= 0.0f || !std::isfinite(halfExtent.y) 
        || halfExtent.y <= 0.0f || !std::isfinite(halfExtent.z) || halfExtent.z <= 0.0f) return std::nullopt;

    JPH::BodyInterface &bodyInterface = physicsSystem->GetBodyInterface();
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    auto shapeResult = shapeSettings.Create();
    if (shapeResult.HasError())
    { /* error */
        return std::nullopt;
    }

    JPH::Quat joltRotation(rotation.x, rotation.y, rotation.z, rotation.w);
    JPH::BodyCreationSettings bodySettings(shapeResult.Get(),
                                           JPH::RVec3(position.x, position.y, position.z),
                                           joltRotation,
                                           JPH::EMotionType::Static,
                                           ObjectLayers::NON_MOVING);

    JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
    if (bodyId.IsInvalid())
    {
        return std::nullopt;
    }
    return bodyId.GetIndexAndSequenceNumber();
}

std::optional<uint32_t> PhysicsWorld::addDynamicBox(glm::vec3 position,
                                                    glm::quat rotation,
                                                    glm::vec3 halfExtent,
                                                    float mass)
{
    if (!physicsSystem)
    { /* error */
        return std::nullopt;
    }

    if (!std::isfinite(mass) || mass <= 0.0f) return std::nullopt;
    if (!std::isfinite(halfExtent.x) || halfExtent.x <= 0.0f || !std::isfinite(halfExtent.y) 
        || halfExtent.y <= 0.0f || !std::isfinite(halfExtent.z) || halfExtent.z <= 0.0f) return std::nullopt;

    JPH::BodyInterface &bodyInterface = physicsSystem->GetBodyInterface();
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    auto shapeResult = shapeSettings.Create();
    if (shapeResult.HasError())
    { /* error */
        return std::nullopt;
    }

    JPH::Quat joltRotation(rotation.x, rotation.y, rotation.z, rotation.w);
    JPH::BodyCreationSettings bodySettings(shapeResult.Get(),
                                           JPH::RVec3(position.x, position.y, position.z),
                                           joltRotation,
                                           JPH::EMotionType::Dynamic,
                                           ObjectLayers::MOVING);

    bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    bodySettings.mMassPropertiesOverride.mMass = mass;

    JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::Activate);
    if (bodyId.IsInvalid())
    {
        return std::nullopt;
    }
    return bodyId.GetIndexAndSequenceNumber();
}

RaycastHitInfo PhysicsWorld::raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance)
{
    RaycastHitInfo result;
    if (!physicsSystem)
        return result;

    JPH::RRayCast ray;
    ray.mOrigin = JPH::RVec3(origin.x, origin.y, origin.z);
    ray.mDirection = JPH::RVec3(direction.x, direction.y, direction.z) * maxDistance;

    JPH::RayCastResult hit;

    // perform query
    const JPH::NarrowPhaseQuery &query = physicsSystem->GetNarrowPhaseQuery();

    if (query.CastRay(ray, hit))
    {
        result.didHit = true;
        result.distance = hit.mFraction * maxDistance;
        JPH::RVec3 hitPos = ray.GetPointOnRay(hit.mFraction);
        result.position = glm::vec3(hitPos.GetX(), hitPos.GetY(), hitPos.GetZ());
        result.bodyId = hit.mBodyID.GetIndexAndSequenceNumber();

        JPH::BodyLockRead lock(physicsSystem->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded())
        {
            const JPH::Body &body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPos);
            result.normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
    }

    return result;
}

float PhysicsWorld::getInterpolationAlpha()
{
    if (!physicsSystem) return 0.0f;

    return std::clamp(static_cast<float>(physicsAccumulator / config.fixedDeltaTime),0.0f, std::nextafter(1.0f, 0.0f));
}

BodyTransform PhysicsWorld::getBodyTransform(uint32_t bodyIdValue)
{
    BodyTransform result;
    if (!physicsSystem)
        return result;

    JPH::BodyInterface &bodyInterface = physicsSystem->GetBodyInterface();
    JPH::BodyID bodyId(bodyIdValue);

    JPH::RVec3 pos = bodyInterface.GetPosition(bodyId);
    JPH::Quat rot = bodyInterface.GetRotation(bodyId);

    result.position =
        glm::vec3(static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()));
    result.rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());

    return result;
}

void PhysicsWorld::setStaticBodyTransform(uint32_t bodyIdValue, const BodyTransform &transform)
{
    if (!physicsSystem || (bodyIdValue & JPH::BodyID::cBroadPhaseBit) != 0)
    {
        return;
    }

    const JPH::BodyID bodyId(bodyIdValue);
    const JPH::Body *body = physicsSystem->GetBodyLockInterface().TryGetBody(bodyId);

    if (!body || !body->IsStatic())
        return;

    const auto &p = transform.position;
    const auto &q = transform.rotation;

    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
        !std::isfinite(q.w) || !std::isfinite(q.x) ||
        !std::isfinite(q.y) || !std::isfinite(q.z))
    {
        return;
    }

    const float lengthSquared = glm::dot(q, q);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f)
        return;

    const glm::quat rotation = glm::normalize(q);

    physicsSystem->GetBodyInterface().SetPositionAndRotationWhenChanged(
        bodyId,
        JPH::RVec3(p.x, p.y, p.z),
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
        JPH::EActivation::DontActivate);
}

uint32_t PhysicsWorld::getBodyCount() const
{
    return physicsSystem ? physicsSystem->GetNumBodies() : 0;
}

void PhysicsWorld::removeBody(uint32_t bodyIdValue)
{
    if (!physicsSystem || (bodyIdValue & JPH::BodyID::cBroadPhaseBit) != 0)
    {
        return;
    }

    const JPH::BodyID bodyId(bodyIdValue);

    const JPH::Body *body = physicsSystem->GetBodyLockInterface().TryGetBody(bodyId);
    if (!body) return;
    
    const bool added = body->IsInBroadPhase();
    auto &bodyInterface = physicsSystem->GetBodyInterface();

    if (added) bodyInterface.RemoveBody(bodyId);

    bodyInterface.DestroyBody(bodyId);
}

void PhysicsWorld::shutdown()
{
    delete physicsSystem;
    physicsSystem = nullptr;

    delete jobSystem;
    jobSystem = nullptr;

    delete tempAllocator;
    tempAllocator = nullptr;

    delete bpLayerInterface;
    bpLayerInterface = nullptr;

    delete objVsBpFilter;
    objVsBpFilter = nullptr;

    delete objLayerPairFilter;
    objLayerPairFilter = nullptr;

    delete contactListener;
    contactListener = nullptr;

    physicsAccumulator = 0.0;
    onFixedUpdate = nullptr;
    onFixedUpdateContext = nullptr;

    if (factory)
    {
        JPH::UnregisterTypes();
        delete factory;
        factory = nullptr;
        JPH::Factory::sInstance = nullptr;
    }
}
} // namespace Cthulhu::Physics
