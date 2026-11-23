#pragma once

#include <cstdint>
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
    [[nodiscard]] std::string setInputMap(ProjectInputMap input);
    [[nodiscard]] std::string_view setDetails(std::string_view name, uint32_t width, uint32_t height);

    bool hasMainScene() const
    {
        return config.mainScene.has_value() || config.mainSceneId.has_value();
    }

    std::optional<std::filesystem::path> resolveResourcePath(std::string_view resourcePath) const;

  private:
    bool saveConfig(ProjectConfig updated);

    ProjectConfig config;

    std::filesystem::path rootPath;
    std::filesystem::path projectFilePath;
};
} // namespace Cthulhu::Project
