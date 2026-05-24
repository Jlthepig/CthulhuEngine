#pragma once

#include "fwd.hpp"
#include "glm.hpp"

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
        static void init(glm::vec3 startPosition, const CharacterConfig& config);
        static void update(glm::vec3 movementInput, bool jump, float deltaTime); // deprecated, use queueInput + fixedUpdate instead
        static void queueInput(glm::vec3 movement, bool jump);
        static void fixedUpdate(float fixedDt);
        static glm::vec3 getPosition();
        static glm::vec3 getInterpolationPosition(float alpha);
        static void destroy();
    };
}