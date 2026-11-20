#include <fstream>
#include <string>

#include "log_utils.hpp"
#include "projectWriter.hpp"

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
    auto temporary = path;
    temporary += "." + Assets::assetIdToString(Assets::generateAssetId()) + ".tmp";

    std::ofstream file(temporary, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        Log::Print("FAILED TO OPEN TEMPORARY PROJECT FILE", "ProjectWriter", LogType::LOG_ERROR);
        return false;
    }

    file << "name = \"" << escapeString(config.name) << "\"\n\n";
    file << "windowWidth = " << config.windowWidth << '\n';
    file << "windowHeight = " << config.windowHeight << '\n';

    if (config.mainSceneId)
        file << "\nmainSceneId = \"" << Assets::assetIdToString(*config.mainSceneId) << "\"\n";
    else if (config.mainScene)
        file << "\nmainScene = \"" << escapeString(*config.mainScene) << "\"\n";

    file.close();
    std::error_code error;
    if (file)
    {
        std::filesystem::rename(temporary, path, error);
        if (!error)
            return true;
    }

    std::filesystem::remove(temporary, error);
    Log::Print("FAILED TO SAVE PROJECT FILE", "ProjectWriter", LogType::LOG_ERROR);
    return false;
}

} // namespace Cthulhu::Project
