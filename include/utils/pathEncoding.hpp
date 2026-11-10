#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Cthulhu::Utils
{
[[nodiscard]] std::optional<std::filesystem::path>
pathFromUtf8(std::string_view text);

[[nodiscard]] std::string
pathToUtf8(const std::filesystem::path &path);
} // namespace Cthulhu::Utils