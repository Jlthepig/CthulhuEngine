#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Cthulhu::Scene
{
    struct EntityId
    {
        uint64_t high{};
        uint64_t low{};

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return high != 0 || low != 0;
        }

        friend constexpr bool operator==(const EntityId&, const EntityId&) noexcept = default;
    };

    struct EntityIdHash
    {
        std::size_t operator()(const EntityId& id) const noexcept
        {
            std::size_t seed = std::hash<uint64_t>{}(id.high);

            seed ^= std::hash<uint64_t>{}(id.low) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
    
    [[nodiscard]] EntityId generateEntityId();
    [[nodiscard]] std::string entityIdToString(EntityId id);
    [[nodiscard]] std::optional<EntityId> entityIdFromString(std::string_view value);

}