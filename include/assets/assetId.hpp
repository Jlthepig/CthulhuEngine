#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Cthulhu::Assets
{

// will refactor later as there is duplicated logic from entityId.hpp

struct AssetId
{
    uint64_t high{};
    uint64_t low{};

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return high != 0 || low != 0;
    }

    friend constexpr bool operator==(const AssetId &, const AssetId &) noexcept = default;
};

struct AssetIdHash
{
    std::size_t operator()(const AssetId &id) const noexcept
    {
        std::size_t seed = std::hash<uint64_t>{}(id.high);
        seed ^= std::hash<uint64_t>{}(id.low) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
};

[[nodiscard]] AssetId generateAssetId();
[[nodiscard]] std::string assetIdToString(AssetId id);
[[nodiscard]] std::optional<AssetId> assetIdFromString(std::string_view value);
} // namespace Cthulhu::Assets