#include "pathEncoding.hpp"

#include <simdjson.h>

namespace Cthulhu::Utils
{
std::optional<std::filesystem::path> pathFromUtf8(std::string_view text)
{
    if (text.empty()) return std::filesystem::path{};

    if (text.find('\0') != std::string_view::npos || !simdjson::validate_utf8(text.data(), text.size()))
        return std::nullopt;

    try
    {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    catch (const std::filesystem::filesystem_error &)
    {
        return std::nullopt;
    }
}

std::string pathToUtf8(const std::filesystem::path &path)
{
    const std::u8string text = path.generic_u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}
} // namespace Cthulhu::Utils