#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assetId.hpp"
#include "assetType.hpp"

namespace Cthulhu::Project
{
class Project;
}

namespace Cthulhu::Assets
{

struct AudioImportSettings
{
    bool stream = false;

    friend bool operator==(const AudioImportSettings &, const AudioImportSettings &) = default;
};
struct AssetRecord
{
    AssetId id;
    std::string path;
    AssetType type = AssetType::Unknown;
    bool missing = false;

    AudioImportSettings audio;
};

// will be located in .cthulhu/assets.json as a project wide database for all assets must be committed or you might
// break your project!
class AssetRegistry
{
  public:
    struct ScanResult
    {
        std::size_t added = 0;
        std::size_t missing = 0;
        std::size_t total = 0;
        bool ok = false;
    };

    bool load(const std::filesystem::path &registryFile);
    bool save() const;

    ScanResult scan(const Project::Project &project);

    bool setAudioImportSettings(AssetId id, const AudioImportSettings &settings);

    // registers the file so it's essentially able to save and it now exists at resource path
    [[nodiscard]] std::optional<AssetId> registerFile(std::string_view resourcePath);
    // changes a record's path << same id >> and saves. will fail if another record uses the path
    bool setPath(AssetId id, std::string_view resourcePath);

    void markMissing(AssetId id);

    bool forget(AssetId id);

    [[nodiscard]] const AssetRecord *findById(AssetId id) const;
    [[nodiscard]] const AssetRecord *findByPath(std::string_view resourcePath) const;

    [[nodiscard]] const std::vector<AssetRecord> &getRecords() const noexcept
    {
        return records;
    }

    [[nodiscard]] bool isUsable() const noexcept
    {
        return usable;
    }

    void clear();

  private:
    std::filesystem::path file;
    bool usable = false;

    std::vector<AssetRecord> records;
    std::unordered_map<AssetId, std::size_t, AssetIdHash> indexById;
    std::unordered_map<std::string, std::size_t> indexByPath;

    void add(AssetId id, std::string path);
    void rebuildIndices();
};
} // namespace Cthulhu::Assets