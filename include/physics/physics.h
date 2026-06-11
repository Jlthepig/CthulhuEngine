#pragma once

#include "fwd.hpp"
#include "glm.hpp"
#include <cstdint>
namespace JPH 
{
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    class ContactListener;
}
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
            JPH::BroadPhaseLayerInterface* bpLayerInterface = nullptr;
            JPH::ObjectVsBroadPhaseLayerFilter* objVsBpFilter = nullptr;
            JPH::ObjectLayerPairFilter* objLayerPairFilter = nullptr;
            JPH::ContactListener* contactListener = nullptr;
    };
}