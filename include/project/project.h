#pragma once

#include "projectConfig.h"

#include <filesystem>
#include <optional>
#include <string_view>
namespace Cthulhu::Project
{
    class Project
    {
        public:
            static std::optional<Project> open(const std::filesystem::path& projectFilePath);

            const ProjectConfig& getConfig() const
            {
                return config;
            }

            const std::filesystem::path& getRootPath() const
            {
                return rootPath;
            }

            const std::filesystem::path& getProjectFilePath() const
            {
                return projectFilePath;
            }

            std::optional<std::filesystem::path> resolveResourcePath(std::string_view resourcePath) const;

        private:

            ProjectConfig config;

            std::filesystem::path rootPath;
            std::filesystem::path projectFilePath;
    };
}