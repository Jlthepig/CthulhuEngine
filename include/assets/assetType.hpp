#pragma once

#include <cstdint>
#include <string_view>

namespace Cthulhu::Assets
{

enum class AssetType: uint8_t
{
    Unknown,
    Model,
    Audio,
    Scene
};

[[nodiscard]] AssetType getAssetType(std::string_view resourcePath);
[[nodiscard]] std::string_view assetTypeName(AssetType type);

}