#include "project.h"
#include "projectParser.h"

#include "log_utils.hpp"

using namespace KalaHeaders::KalaLog;

namespace Cthulhu::Project 
{
    static bool isInsideProjectRoot(const std::filesystem::path& root, const std::filesystem::path& path) 
    {
        auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());
        
        if (relative.empty()) return false; 
        
        return *relative.begin() != "..";
    }

    std::optional<std::filesystem::path> Project::resolveResourcePath(std::string_view resourcePath) const
    {
        constexpr std::string_view prefix = "res://";

        if (!resourcePath.starts_with(prefix))
        {
            Log::Print("RESOURCE PATH MUST START WITH 'res://': " + std::string(resourcePath), "Project",LogType::LOG_ERROR);
            return std::nullopt;
        }

        std::string_view relativePart = resourcePath.substr(prefix.size());

        if (relativePart.empty())
        {
            return rootPath;
        }

        std::filesystem::path relativePath {std::string(relativePart)};

        if (relativePath.is_absolute() || relativePath.has_root_name() || relativePath.has_root_directory())
        {
            Log::Print("INVALID RESOURCE PATH: " + std::string(resourcePath), "Project",LogType::LOG_ERROR);
            return std::nullopt;
        }

        std::filesystem::path resolved = (rootPath / relativePath).lexically_normal();

        if (!isInsideProjectRoot(rootPath, resolved))
        {
            Log::Print("RESOURCE PATH ESCAPES PROJECT ROOT: " + std::string(resourcePath), "Project",LogType::LOG_ERROR);
            return std::nullopt;
        }

        return resolved;

    }

     std::optional<Project> Project::open(const std::filesystem::path& projectFilePath)
     {
        std::error_code error;

        std::filesystem::path absolutePath = std::filesystem::absolute(projectFilePath,error);

        if (error)
        {
            Log::Print("FAILED TO RESOLVE PROJECT PATH: " + projectFilePath.string(),"Project", LogType::LOG_ERROR);
            return std::nullopt;
        }

        absolutePath = absolutePath.lexically_normal();

        std::filesystem::path canonicalProjectFile = std::filesystem::canonical(absolutePath, error);

        if (error)
        {
            Log::Print(
                "FAILED TO CANONICALIZE PROJECT PATH: " + absolutePath.string(),
                "Project",
                LogType::LOG_ERROR);

            return std::nullopt;
        }

        absolutePath = std::move(canonicalProjectFile);

        if (absolutePath.filename() != "project.cthulhu")
        {
            Log::Print("PROJECT FILE MUST BE NAMED 'project.cthulhu'", "Project",LogType::LOG_ERROR);
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