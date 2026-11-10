#include <limits>
#include <utility>

#include "fileReader.hpp"
#include "pathEncoding.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Utils
{
std::string FileReader::readFile(const std::string &file)
{
    const auto native = pathFromUtf8(file);

    std::ifstream input;

    if (native) input.open(*native, std::ios::binary | std::ios::ate);

    if (!input.is_open())
    {
        Log::Print("FILE MISSING DOUBLE CHECK PATH: " + file, "File", LogType::LOG_ERROR);
        return "";
    }

    const std::streamoff size = input.tellg();
    if (size < 0 ||
        std::cmp_greater(size, std::string{}.max_size()) ||
        std::cmp_greater(size, std::numeric_limits<std::streamsize>::max()))
    {
        Log::Print("INVALID FILE SIZE: " + file, "File", LogType::LOG_ERROR);
        return {};
    }

    std::string text(static_cast<std::size_t>(size), '\0');
    input.seekg(0);

    if (!input.read(text.data(), static_cast<std::streamsize>(size)))
    {
        Log::Print("FAILED TO READ FILE: " + file, "File", LogType::LOG_ERROR);
        return {};
    }

    return text;
}
} // namespace Cthulhu::Utils
