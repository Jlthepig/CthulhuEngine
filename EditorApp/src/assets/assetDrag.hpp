#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "OctoGui/Vec2.h"
#include "assetHandle.hpp"
#include "assetId.hpp"
namespace Cthulhu::Editor
{
    struct AssetDragState
    {
        Assets::AssetId asset{};
        Assets::ModelHandle previewModel{};
        bool previewLoadAttempted = false;
        std::filesystem::path projectFile;
        std::string name;
        std::uint64_t generation{};
        octogui::Vec2 press{};
        bool active = false;
        bool ownsInput = false;
    };

    struct AssetDropFeedback
    {
        bool accepted = false;
        std::string message = "Drop on a matching asset field.";
        bool failed = false;
    };
    
}