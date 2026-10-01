#pragma once

#include <cstddef>
#include <filesystem>
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
struct AssetRecord
{
    AssetId id;
    std::string path;
    AssetType type = AssetType::Unknown;
    bool missing = false;
};

// will be located in .cthulhu/assets.json as a project wide database for all assets must be committed or you might break your project!
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
        std::unordered_map<std::string, std::size_t, AssetIdHash> indexByPath;

        void add(AssetId id, std::string path);
        void rebuildIndices();
};
} // namespace Cthulhu::Assets