#pragma once

#include <filesystem>

#include "projectConfig.hpp"
namespace Cthulhu::Project
{
class ProjectWriter
{
  public:
    static bool write(const std::filesystem::path &path, const ProjectConfig &config);
};
} // namespace Cthulhu::Project