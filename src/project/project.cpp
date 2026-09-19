#include "project.h"
#include "projectParser.h"

#include "log_utils.hpp"

using namespace KalaHeaders::KalaLog;

namespace Cthulhu::Project 
{
     std::optional<Project> Project::open(const std::filesystem::path& projectFilePath)
     {
        std::error_code error;

        std::filesystem::path absolutePath = std::filesystem::absolute(projectFilePath,error);

        if (error)
        {
            Log::Print("FAILED TO RESOLVE PROJECT PATH" + projectFilePath.string(), 
            "Project", LogType::LOG_ERROR);

            return std::nullopt;
        }

        absolutePath = absolutePath.lexically_normal();

        if (absolutePath.filename() != "project.cthulhu")
        {
            Log::Print("FAILED TO RESOLVE PROJECT PATH", "Project",LogType::LOG_ERROR);

            return std::nullopt;
        }

        if (!std::filesystem::is_regular_file(absolutePath, error) || error)
        {
            Log::Print("PROJECT FILE DOES NOT EXIST: " + absolutePath.string(), "Project",LogType::LOG_ERROR);

            return std::nullopt;
        }

        auto parsedConfig = ProjectParser::parse(absolutePath.string());

        if (!parsedConfig)
        {
            return std::nullopt;
        }

        Project project;
        project.config = std::move(*parsedConfig);
        project.projectFilePath = absolutePath;
        project.rootPath = absolutePath.parent_path();

        Log::Print("PROJECT OPENED: " + project.config.name, "Project",LogType::LOG_SUCCESS);

        return project;
     }
}