#include <random>
#include <charconv>
#include <iomanip>
#include <sstream>

#include "entityId.hpp"

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

    std::string entityIdToString(EntityId id)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0') << std::setw(16) << id.high << std::setw(16) << id.low;
        return stream.str();
    }

    std::optional<EntityId> entityIdFromString(std::string_view value)
    {
        if (value.size() != 32)
        {
            return std::nullopt;
        }
        
        uint64_t high{};
        uint64_t low{};

        const auto highPart = value.substr(0,16);
        const auto lowPart = value.substr(16,16);

        auto[highEnd, highError] = std::from_chars(highPart.data(), highPart.data() + highPart.size(), high, 16);
        auto[lowEnd, lowError] = std::from_chars(lowPart.data(), lowPart.data() + lowPart.size(), low, 16);

        if (highError != std::errc{} || lowError != std::errc{} || highEnd != highPart.data() + highPart.size() || lowEnd != lowPart.data() + lowPart.size())
        {
            return std::nullopt;
        }

        EntityId id{high,low};
        if (!id.isValid())
        {
            return std::nullopt;
        }

        return id;
    }
}