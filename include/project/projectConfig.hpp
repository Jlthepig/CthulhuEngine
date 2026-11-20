#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "assetId.hpp"

namespace Cthulhu::Project
{
struct ProjectConfig
{
    std::string name{"Untitled Project"};

    uint32_t windowWidth{1280};
    uint32_t windowHeight{720};

    std::optional<std::string> mainScene;
    std::optional<Assets::AssetId> mainSceneId;
};
} // namespace Cthulhu::Project
