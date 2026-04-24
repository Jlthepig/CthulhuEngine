#pragma once

#include "glm.hpp"

namespace Cthulhu::Physics
{
    class CharacterController
    {
    public:
        static void init(glm::vec3 startPosition);
        static void update(glm::vec3 movementInput, bool jump, float deltaTime);
        static glm::vec3 getPosition();
        static void destroy();
    };
}