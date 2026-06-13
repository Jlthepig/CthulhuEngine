#pragma once

#include "fwd.hpp"
#include "glm.hpp"

namespace flecs { struct entity; }
namespace Cthulhu::Physics { class PhysicsWorld; }
namespace Cthulhu::Physics
{
    struct CharacterConfig
    {
        float gravity = -9.81f;
        float jumpVelocity = 5.0f;
        float capsuleRadius = 0.3f;
        float capsuleHeight = 2.0f;
        float maxWalkableSlope = 45.0f; // Degrees
        float maxPushStrength = 100.0f;
    };
    class CharacterController
    {
    public:
        static void create(flecs::entity e, glm::vec3 startPosition, const CharacterConfig& config, PhysicsWorld& physicsWorld);
        static void destroy(flecs::entity e);

        static void teleport(flecs::entity e, const glm::vec3& pos);
    };
}