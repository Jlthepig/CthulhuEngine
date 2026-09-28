#pragma once

#include <cstdint>

namespace Cthulhu::Rendering
{
struct Model;
}
namespace Cthulhu::Assets
{
    template <typename T>
    struct AssetHandle
    {
        uint32_t index{};
        uint32_t generation{};

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return generation != 0;
        }

        friend constexpr bool operator==(const AssetHandle & , const AssetHandle &) noexcept = default;
    };

    using ModelHandle = AssetHandle<Rendering::Model>;
}