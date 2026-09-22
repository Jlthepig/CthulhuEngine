#include <sstream>

#include "fileReader.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Utils
{
std::string FileReader::readFile(const std::string &file)
{
    std::ifstream input(file);

    if (!input.is_open())
    {
        Log::Print("FILE MISSING DOUBLE CHECK PATH: " + file, "File", LogType::LOG_ERROR);
        return "";
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}
} // namespace Cthulhu::Utils