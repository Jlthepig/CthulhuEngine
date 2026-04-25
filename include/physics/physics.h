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

    class Physics
    {
        public:
            static void init();
            static void step(float deltaTime);
            static void shutdown();
            static void createGroundPlane();

            static uint32_t addStaticBox(glm::vec3 position, glm::vec3 halfExtent);
            static uint32_t addDynamicBox(glm::vec3 position, glm::vec3 halfextent, float mass);
            static BodyTransform getBodyTransform(uint32_t bodyId);
    };

}