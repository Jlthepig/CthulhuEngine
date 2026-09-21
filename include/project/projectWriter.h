#pragma once

#include "projectConfig.h"

#include <filesystem>

namespace Cthulhu::Project
{
    class ProjectWriter
    {
    public:
        static bool write(const std::filesystem::path& path, const ProjectConfig& config);
    };
}