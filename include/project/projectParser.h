#pragma once

#include "projectConfig.h"

#include <optional>
#include <string>

namespace Cthulhu::Project
{
    class ProjectParser
    {
        public:
            static std::optional<ProjectConfig> parse(const std::string& path);
    };
}