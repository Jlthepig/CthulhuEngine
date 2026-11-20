#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "projectConfig.hpp"

namespace Cthulhu::Assets
{
class AssetRegistry;
struct AssetRecord;
}

namespace Cthulhu::Project
{
class Project
{
  public:
    static std::optional<Project> open(const std::filesystem::path &projectFilePath);
    static std::optional<Project> createProject(const std::filesystem::path &rootPath, const ProjectConfig &config);

    const ProjectConfig &getConfig() const
    {
        return config;
    }

    const std::filesystem::path &getRootPath() const
    {
        return rootPath;
    }

    const std::filesystem::path &getProjectFilePath() const
    {
        return projectFilePath;
    }

    [[nodiscard]] const std::string *getMainScene(const Assets::AssetRegistry &registry) const;
    [[nodiscard]] bool isMainScene(const Assets::AssetRecord &record, const Assets::AssetRegistry &registry) const;
    [[nodiscard]] bool setMainScene(std::optional<Assets::AssetId> id);

    bool hasMainScene() const
    {
        return config.mainScene.has_value() || config.mainSceneId.has_value();
    }

    std::optional<std::filesystem::path> resolveResourcePath(std::string_view resourcePath) const;

  private:
    ProjectConfig config;

    std::filesystem::path rootPath;
    std::filesystem::path projectFilePath;
};
} // namespace Cthulhu::Project
