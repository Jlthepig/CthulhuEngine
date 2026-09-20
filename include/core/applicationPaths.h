#pragma once

#include <filesystem>
#include <optional>

namespace Cthulhu::Core
{
    std::optional<std::filesystem::path> getExecutableDirectory();
}