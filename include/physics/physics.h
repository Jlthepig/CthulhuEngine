#pragma once

#include "fwd.hpp"
#include "glm.hpp"
#include <cstdint>

namespace Cthulhu::Physics
{
    struct BodyTransform
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f); // euler radians
    };
    struct PhysicsConfig
    {
        // Core System
        int tempAllocatorSizeMB = 10;
        int threadCount = 1;
        int maxBodies = 1024;
        int maxBodyPairs = 1024;
        int maxContactConstraints = 1024;
        
        // Simulation
        float fixedDeltaTime = 1.0f / 60.0f;
        int collisionSteps = 1;
        float maxDeltaTime = 0.25f; // Spiral of death cap

        // Ground Plane
        float groundWidth = 100.0f;
        float groundHeight = 0.5f;
        float groundDepth = 100.0f;
        float groundFriction = 0.8f;
    };
    class Physics
    {
        public:
            static void init(const PhysicsConfig& config);
            static void step(float deltaTime);
            static float getInterpolationAlpha();
            static void shutdown();
            static void createGroundPlane();

            static uint32_t addStaticBox(glm::vec3 position, glm::vec3 halfExtent);
            static uint32_t addDynamicBox(glm::vec3 position, glm::vec3 halfextent, float mass);
            static BodyTransform getBodyTransform(uint32_t bodyId);
    };
}