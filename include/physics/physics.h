#pragma once

#include "glm.hpp"

namespace Cthulhu::Physics
{

    class Physics
    {
        public:
        static void init();
        static void step(float deltaTime);

        static void createGroundPlane();

        static void shutdown();
    };

}