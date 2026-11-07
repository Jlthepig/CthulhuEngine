#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assetId.hpp"
#include "assetType.hpp"

namespace Cthulhu::Assets
{
class AssetRegistry;
}

namespace Cthulhu::Project
{
class Project;
}

namespace Cthulhu::Editor
{
struct ContentBrowserEntry
{
    std::string path;
    Assets::AssetId assetId{};
    Assets::AssetType type = Assets::AssetType::Unknown;
    bool folder = false;
    bool missing = false;

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::string_view parent() const noexcept;
};

class ContentBrowserIndex
{
  public:
    [[nodiscard]] bool rebuild(const Project::Project &project, const Assets::AssetRegistry &registry, std::string &error);

    [[nodiscard]] std::span<const ContentBrowserEntry> entries() const noexcept
    {
        return cached;
    }

    [[nodiscard]] std::optional<std::size_t> findFolder(std::string_view path) const;

    // no filters and case insensitive
    [[nodiscard]] bool query(std::string_view directory, std::string_view search, std::optional<Assets::AssetType> type, std::vector<std::size_t> &out) const;

  private:
    std::vector<ContentBrowserEntry> cached;
    std::vector<std::string> searchKeys;
    std::size_t folderCount{};
};
} // namespace Cthulhu::Editor