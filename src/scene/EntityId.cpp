#include <random>

#include "scene/EntityId.hpp"

namespace Cthulhu::Scene
{
    EntityId generateEntityId()
    {
        static thread_local std::mt19937_64 generator = []()
        {
            std::random_device randomDevice;

            std::seed_seq seed
            {
                randomDevice(),
                randomDevice(),
                randomDevice(),
                randomDevice(),
                randomDevice(),
                randomDevice(),
                randomDevice(),
                randomDevice()
            };

            return std::mt19937_64(seed);
        }();

        EntityId id;

        while (!id.isValid())
        {
            id.high = generator();
            id.low = generator();
        }

        return id;
    }
}