#include <cctype>
#include <string>

#include "assetType.hpp"

namespace Cthulhu::Assets
{
AssetType getAssetType(std::string_view resourcePath)
{
    const size_t dot = resourcePath.find_last_of('.');
    const size_t separator = resourcePath.find_last_of("/\\");

    if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator))
    {
        return AssetType::Unknown;
    }

    std::string extension(resourcePath.substr(dot));
    for (char &c : extension)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (extension == ".glb")
    {
        return AssetType::Model;
    }

    if (extension == ".wav" || extension == ".mp3" || extension == ".flac")
    {
        return AssetType::Audio;
    }

    if (extension == ".scene")
    {
        return AssetType::Scene;
    }

    return AssetType::Unknown;
}

std::string_view assetTypeName(AssetType type)
{
    switch (type)
    {
    case AssetType::Model:
        return "Model";
    case AssetType::Audio:
        return "Audio";
    case AssetType::Scene:
        return "Scene";
    case AssetType::Unknown:
    default:
        return "Unknown";
    }
}
} // namespace Cthulhu::Assets