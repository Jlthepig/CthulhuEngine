#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assetHandle.hpp"
#include "assetRegistry.hpp"
#include "assetSlotTable.hpp"
#include "assetType.hpp"
#include "model.hpp"

namespace Cthulhu::Project
{
class Project;
}

namespace Cthulhu::Assets
{
[[nodiscard]] std::optional<std::string> normaliseResourcePath(std::string_view resourcePath);

class AssetManager
{
  public:
    AssetManager() = default;
    ~AssetManager();

    AssetManager(const AssetManager &) = delete;
    AssetManager &operator=(const AssetManager &) = delete;

    void setProject(const Project::Project *activeProject) noexcept
    {
        project = activeProject;
    }

    AssetRegistry::ScanResult refreshRegistry();

    [[nodiscard]] std::optional<AssetId> importFile(const std::filesystem::path &sourceFile,
                                                    std::string_view destination);

    [[nodiscard]] bool moveFile(AssetId id, std::string_view destination);

    [[nodiscard]] bool deleteFile(AssetId id);

    [[nodiscard]] bool setAudioImportSettings(AssetId id, const AudioImportSettings &settings);

    [[nodiscard]] AssetRegistry &getRegistry() noexcept
    {
        return registry;
    }

    [[nodiscard]] const AssetRegistry &getRegistry() const noexcept
    {
        return registry;
    }

    [[nodiscard]] ModelHandle loadModel(std::string_view resourcePath);

    [[nodiscard]] Rendering::Model *getModel(ModelHandle handle);
    [[nodiscard]] const Rendering::Model *getModel(ModelHandle handle) const;

    [[nodiscard]] ModelHandle acquireModel(std::string_view resourcePath);

    [[nodiscard]] AudioClipHandle loadAudioClip(std::string_view resourcePath);
    [[nodiscard]] AudioClipHandle acquireAudioClip(std::string_view resourcePath);
    void releaseAudioClip(AudioClipHandle handle);

    [[nodiscard]] const Core::AudioClipData *getAudioClip(AudioClipHandle handle) const;
    [[nodiscard]] uint32_t getAudioClipRefCount(AudioClipHandle handle) const;
    [[nodiscard]] uint32_t getTotalAudioClipRefCount() const noexcept;
    [[nodiscard]] std::size_t getLoadedAudioClipCount() const noexcept;

    std::size_t collectUnusedAudioClips();

    std::size_t collectUnusedAssets();

    void releaseModel(ModelHandle handle);

    [[nodiscard]] uint32_t getModelRefCount(ModelHandle handle) const;
    [[nodiscard]] uint32_t getTotalModelRefCount() const noexcept;

    [[nodiscard]] std::size_t getLoadedModelCount() const noexcept;

    std::size_t collectUnusedModels();

    void shutdown();

  private:
    const Project::Project *project = nullptr;

    AssetRegistry registry;
    bool registryLoaded = false;

    AssetSlotTable modelTable;
    std::vector<std::unique_ptr<Rendering::Model>> models;

    AssetSlotTable audioClipTable;
    std::vector<Core::AudioClipData *> audioClips;

    [[nodiscard]] std::optional<std::string> makeAssetKey(std::string_view resourcePath, AssetType expected) const;
};
} // namespace Cthulhu::Assets
