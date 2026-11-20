#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace Cthulhu::Project
{
class Project;
}

namespace Cthulhu::Editor
{
[[nodiscard]] std::optional<std::string> sceneResourcePath(const Project::Project &project, const std::filesystem::path &selected);
}
