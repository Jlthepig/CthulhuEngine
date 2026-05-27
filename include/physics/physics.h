#pragma once

#include "fwd.hpp"
#include "glm.hpp"
#include <cstdint>

// Jolt includes
#include "Jolt/Jolt.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "Jolt/Physics/Collision/ContactListener.h"

namespace Cthulhu::Physics
{
    struct BodyTransform
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f); // euler radians
    };

    struct PhysicsConfig
    {
        int tempAllocatorSizeMB = 10;
        int threadCount = 1;
        int maxBodies = 1024;
        int maxBodyPairs = 1024;
        int maxContactConstraints = 1024;
        float fixedDeltaTime = 1.0f / 60.0f;
        int collisionSteps = 1;
        float maxDeltaTime = 0.25f;
        float groundWidth = 100.0f;
        float groundHeight = 0.5f;
        float groundDepth = 100.0f;
        float groundFriction = 0.8f;
    };

    // Jolt Layer Implementations
    namespace ObjectLayers
    {
        static constexpr JPH::ObjectLayer MOVING = 0;
        static constexpr JPH::ObjectLayer NON_MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYER = 2;
    }
    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer MOVING(0);
        static constexpr JPH::BroadPhaseLayer NON_MOVING(1);
        static constexpr JPH::uint NUM_LAYER(2);
    }

    class BPLayerInterface : public JPH::BroadPhaseLayerInterface
    {
    public:
        virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYER; }
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            if (layer == ObjectLayers::NON_MOVING) return BroadPhaseLayers::NON_MOVING;
            return BroadPhaseLayers::MOVING;
        }
    };
    class ObjVsBPLayerFilter : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer objectLayer, JPH::BroadPhaseLayer bpLayer) const override
        {
            if (objectLayer == ObjectLayers::NON_MOVING) return bpLayer == BroadPhaseLayers::MOVING;
            return true;
        }
    };
    class ObjLayerPairFilter : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
        {
            if (layer1 == ObjectLayers::NON_MOVING && layer2 == ObjectLayers::NON_MOVING) return false;
            return true;
        }
    };

    class ContactListener : public JPH::ContactListener {};
    class PhysicsWorld
    {
        public:
            void init(const PhysicsConfig& config);
            void step(float deltaTime);
            float getInterpolationAlpha();
            void shutdown();
            void createGroundPlane();

            uint32_t addStaticBox(glm::vec3 position, glm::vec3 halfExtent);
            uint32_t addDynamicBox(glm::vec3 position, glm::vec3 halfextent, float mass);
            BodyTransform getBodyTransform(uint32_t bodyId);

            // Getters for Character Controller
            JPH::PhysicsSystem* getPhysicsSystem() const { return physicsSystem; }
            JPH::TempAllocatorImpl* getTempAllocator() const { return tempAllocator; }
            const PhysicsConfig& getConfig() const { return config; }

        private:
            PhysicsConfig config;
            float physicsAccumulator = 0.0f;

            // Jolt State
            JPH::TempAllocatorImpl* tempAllocator = nullptr;
            JPH::JobSystemThreadPool* jobSystem = nullptr;
            JPH::PhysicsSystem* physicsSystem = nullptr;

            // Layer interfaces (Concrete classes, not abstract!)
            BPLayerInterface bpLayerInterface;
            ObjVsBPLayerFilter objVsBpFilter;
            ObjLayerPairFilter objLayerPairFilter;
            ContactListener contactListener;
    };
}