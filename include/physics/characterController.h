#pragma once

#include "fwd.hpp"
#include "glm.hpp"

namespace Cthulhu::Physics
{
    class CharacterController
    {
    public:
        static void init(glm::vec3 startPosition);
        static void update(glm::vec3 movementInput, bool jump, float deltaTime); // deprecated, use queueInput + fixedUpdate instead
        static void queueInput(glm::vec3 movement, bool jump);
        static void fixedUpdate(float fixedDt);
        static glm::vec3 getPosition();
        static glm::vec3 getInterpolationPosition(float alpha);
        static void destroy();
    };
}