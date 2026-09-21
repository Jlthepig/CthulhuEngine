#include "projectWriter.h"
#include "log_utils.hpp"

#include <fstream>
#include <string>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Project
{

static std::string escapeString(const std::string &value)
{
    std::string result;
    result.reserve(value.size());

    for (char c : value)
    {
        switch (c)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
            break;
        }
    }

    return result;
}

bool ProjectWriter::write(const std::filesystem::path &path, const ProjectConfig &config)
{
    std::ofstream file(path, std::ios::out | std::ios::trunc);

    if (!file.is_open())
    {
        Log::Print("FAILED TO CREATE PROJECT FILE: " + path.string(), "ProjectWriter", LogType::LOG_ERROR);
        return false;
    }

    file << "name = \"" << escapeString(config.name) << "\"\n\n";
    file << "windowWidth = " << config.windowWidth << '\n';
    file << "windowHeight = " << config.windowHeight << '\n';

    if (config.mainScene)
    {
        file << "\nmainScene = \"" << escapeString(*config.mainScene) << "\"\n";
    }

    if (!file)
    {
        Log::Print("FAILED WHILE WRITING PROJECT FILE: " + path.string(), "ProjectWriter", LogType::LOG_ERROR);
        return false;
    }

    return true;
}

} // namespace Cthulhu::Project