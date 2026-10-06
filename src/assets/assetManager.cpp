#include <algorithm>
#include <filesystem>

#include "assetManager.hpp"
#include "audio.hpp"
#include "modelLoader.hpp"
#include "project.hpp"
#include "platform.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Assets
{
std::optional<std::string> normaliseResourcePath(std::string_view resourcePath)
{
    constexpr std::string_view prefix = "res://";

    if (!resourcePath.starts_with(prefix))
    {
        return std::nullopt;
    }

    std::string relative(resourcePath.substr(prefix.size()));
    std::replace(relative.begin(), relative.end(), '\\', '/');

    const std::filesystem::path path = std::filesystem::path(relative).lexically_normal();

    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    {
        return std::nullopt;
    }

    std::string normalized = path.generic_string();

    if (normalized == ".")
    {
        normalized.clear();
    }

    while (!normalized.empty() && normalized.back() == '/')
    {
        normalized.pop_back();
    }

    if (normalized == ".." || normalized.starts_with("../"))
    {
        return std::nullopt;
    }

    return std::string(prefix) + normalized;
}

AssetManager::~AssetManager()
{
    if (modelTable.liveCount() != 0 || audioClipTable.liveCount() != 0)
    {
        Log::Print("ASSET MANAGER DESTROYED WITHOUT SHUTDOWN", "AssetManager", LogType::LOG_WARNING);
    }
}

AssetRegistry::ScanResult AssetManager::refreshRegistry()
{
    if (!project)
    {
        return {};
    }

    if (!registryLoaded)
    {
        registry.load(project->getRootPath() / ".cthulhu" / "assets.json");
        registryLoaded = true;
    }

    return registry.scan(*project);
}

ModelHandle AssetManager::loadModel(std::string_view resourcePath)
{
    auto key = makeAssetKey(resourcePath, AssetType::Model);
    if (!key)
    {
        return {};
    }

    if (auto existing = modelTable.find(*key))
    {
        return ModelHandle{existing->index, existing->generation};
    }

    if (!project)
    {
        Log::Print("CANNOT LOAD MODEL WITHOUT A PROJECT: " + *key, "AssetManager", LogType::LOG_ERROR);
        return {};
    }

    auto filePath = project->resolveResourcePath(*key);
    if (!filePath)
    {
        return {};
    }

    auto loaded = Rendering::ModelLoader::loadGltf(filePath->string());
    if (!loaded)
    {
        Log::Print("FAILED TO LOAD MODEL: " + *key, "AssetManager", LogType::LOG_ERROR);
        return {};
    }

    const auto slot = modelTable.allocate(*key);
    if (slot.index >= models.size())
    {
        models.resize(slot.index + 1);
    }
    models[slot.index] = std::make_unique<Rendering::Model>(std::move(*loaded));

    Log::Print("Loaded model: " + *key, "AssetManager", LogType::LOG_INFO);
    return ModelHandle{slot.index, slot.generation};
}

ModelHandle AssetManager::acquireModel(std::string_view resourcePath)
{
    const ModelHandle handle = loadModel(resourcePath);
    if (handle.isValid())
    {
        modelTable.addRef(handle.index, handle.generation);
    }
    return handle;
}

void AssetManager::releaseModel(ModelHandle handle)
{
    modelTable.release(handle.index, handle.generation);
}

Rendering::Model *AssetManager::getModel(ModelHandle handle)
{
    return modelTable.isAlive(handle.index, handle.generation) ? models[handle.index].get() : nullptr;
}

const Rendering::Model *AssetManager::getModel(ModelHandle handle) const
{
    return modelTable.isAlive(handle.index, handle.generation) ? models[handle.index].get() : nullptr;
}

uint32_t AssetManager::getModelRefCount(ModelHandle handle) const
{
    return modelTable.refCount(handle.index, handle.generation);
}

uint32_t AssetManager::getTotalModelRefCount() const noexcept
{
    return modelTable.totalRefCount();
}

std::size_t AssetManager::getLoadedModelCount() const noexcept
{
    return modelTable.liveCount();
}

std::size_t AssetManager::collectUnusedModels()
{
    const auto collected = modelTable.collectUnreferenced();

    for (const auto &entry : collected)
    {
        if (auto &model = models[entry.index])
        {
            model->destroy();
            model.reset();
        }
        Log::Print("Unloaded model: " + entry.key, "AssetManager", LogType::LOG_INFO);
    }

    return collected.size();
}

std::optional<std::string> AssetManager::makeAssetKey(std::string_view resourcePath, AssetType expected) const
{
    auto key = normaliseResourcePath(resourcePath);
    if (!key)
    {
        Log::Print("INVALID RESOURCE PATH: " + std::string(resourcePath), "AssetManager", LogType::LOG_ERROR);
        return std::nullopt;
    }

    if (const AssetType type = getAssetType(*key); type != expected)
    {
        Log::Print("EXPECTED " + std::string(assetTypeName(expected)) + " ASSET, GOT " +
                       std::string(assetTypeName(type)) + ": " + *key,
                   "AssetManager", LogType::LOG_ERROR);
        return std::nullopt;
    }

    return key;
}

AudioClipHandle AssetManager::loadAudioClip(std::string_view resourcePath)
{
    auto key = makeAssetKey(resourcePath, AssetType::Audio);
    if (!key)
    {
        return {};
    }

    if (auto existing = audioClipTable.find(*key))
    {
        return AudioClipHandle{existing->index, existing->generation};
    }

    if (!project)
    {
        Log::Print("CANNOT LOAD AUDIO CLIP WITHOUT A PROJECT: " + *key, "AssetManager", LogType::LOG_ERROR);
        return {};
    }

    auto filePath = project->resolveResourcePath(*key);
    if (!filePath)
    {
        return {};
    }

    AudioImportSettings settings;
    if (const auto *record = registry.findByPath(*key))
    {
        settings = record->audio;
    }

    Core::AudioClipData *clip = Core::Audio::loadClip(filePath->string(), settings.stream);
    if (!clip)
    {
        Log::Print("FAILED TO LOAD AUDIO CLIP: " + *key, "AssetManager", LogType::LOG_ERROR);
        return {};
    }

    const auto slot = audioClipTable.allocate(*key);
    if (slot.index >= audioClips.size())
    {
        audioClips.resize(slot.index + 1, nullptr);
    }
    audioClips[slot.index] = clip;

        Log::Print("Loaded audio clip: " + *key + (settings.stream ? " (streamed)" : ""), "AssetManager",LogType::LOG_INFO);
    return AudioClipHandle{slot.index, slot.generation};
}

AudioClipHandle AssetManager::acquireAudioClip(std::string_view resourcePath)
{
    const AudioClipHandle handle = loadAudioClip(resourcePath);
    if (handle.isValid())
    {
        audioClipTable.addRef(handle.index, handle.generation);
    }
    return handle;
}

void AssetManager::releaseAudioClip(AudioClipHandle handle)
{
    audioClipTable.release(handle.index, handle.generation);
}

const Core::AudioClipData *AssetManager::getAudioClip(AudioClipHandle handle) const
{
    return audioClipTable.isAlive(handle.index, handle.generation) ? audioClips[handle.index] : nullptr;
}

uint32_t AssetManager::getAudioClipRefCount(AudioClipHandle handle) const
{
    return audioClipTable.refCount(handle.index, handle.generation);
}

uint32_t AssetManager::getTotalAudioClipRefCount() const noexcept
{
    return audioClipTable.totalRefCount();
}

std::size_t AssetManager::getLoadedAudioClipCount() const noexcept
{
    return audioClipTable.liveCount();
}

std::size_t AssetManager::collectUnusedAudioClips()
{
    const auto collected = audioClipTable.collectUnreferenced();

    for (const auto &entry : collected)
    {
        Core::Audio::destroyClip(audioClips[entry.index]);
        audioClips[entry.index] = nullptr;
        Log::Print("Unloaded audio clip: " + entry.key, "AssetManager", LogType::LOG_INFO);
    }

    return collected.size();
}

std::size_t AssetManager::collectUnusedAssets()
{
    return collectUnusedModels() + collectUnusedAudioClips();
}

void AssetManager::shutdown()
{
    for (auto &model : models)
    {
        if (model)
        {
            model->destroy();
        }
    }

    models.clear();
    modelTable.clear();

    for (auto *clip : audioClips)
    {
        Core::Audio::destroyClip(clip);
    }

    audioClips.clear();
    audioClipTable.clear();
    registry.clear();
    registryLoaded = false;
    project = nullptr;
}

namespace
{

bool isInHiddenFolder(std::string_view key)
{
    const std::filesystem::path relative(std::string(key.substr(std::string_view("res://").size())));

    for (const auto &part : relative.parent_path())
    {
        const std::string name = part.string();
        if (!name.empty() && name.front() == '.')
        {
            return true;
        }
    }
    return false;
}

std::optional<AssetId> importFailed(const std::string &why)
{
    Log::Print("IMPORT FAILED: " + why, "AssetManager", LogType::LOG_ERROR);
    return std::nullopt;
}

bool moveFailed(const std::string &why)
{
    Log::Print("MOVE FAILED: " + why, "AssetManager", LogType::LOG_ERROR);
    return false;
}


bool deleteFailed(const std::string &why)
{
    Log::Print("DELETE FAILED: " + why, "AssetManager", LogType::LOG_ERROR);
    return false;
}


bool settingsFailed(const std::string &why)
{
    Log::Print("IMPORT SETTINGS FAILED: " + why, "AssetManager", LogType::LOG_ERROR);
    return false;
}

}

std::optional<AssetId> AssetManager::importFile(const std::filesystem::path &sourceFile, std::string_view destination)
{
    if (!project)
    {
        return importFailed("NO PROJECT");
    }

    if (!registry.isUsable())
    {
        return importFailed("ASSET REGISTRY IS UNUSABLE");
    }

    auto key = normaliseResourcePath(destination);
    if (!key)
    {
        return importFailed("BAD DESTINATION PATH: " + std::string(destination));
    }

    const AssetType type = getAssetType(*key);
    if (type == AssetType::Unknown)
    {
        return importFailed("UNKNOWN ASSET TYPE: " + *key);
    }

    if (getAssetType(sourceFile.generic_string()) != type)
    {
        return importFailed("SOURCE AND DESTINATION TYPES DIFFER: " + *key);
    }

    if (isInHiddenFolder(*key))
    {
        return importFailed("DESTINATION IS IN A HIDDEN FOLDER: " + *key);
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(sourceFile, error))
    {
        return importFailed("SOURCE IS NOT A FILE: " + sourceFile.string());
    }

    auto target = project->resolveResourcePath(*key);
    if (!target)
    {
        return importFailed("DESTINATION IS OUTSIDE THE PROJECT: " + *key);
    }

    if (std::filesystem::exists(*target, error))
    {
        return importFailed("DESTINATION ALREADY EXISTS: " + *key);
    }

    std::filesystem::create_directories(target->parent_path(), error);
    if (error)
    {
        return importFailed("CANNOT CREATE FOLDER FOR: " + *key);
    }

    if (!std::filesystem::copy_file(sourceFile, *target, std::filesystem::copy_options::none, error) || error)
    {
        return importFailed("COPY FAILED: " + error.message());
    }

    auto id = registry.registerFile(*key);
    if (!id)
    {
        std::filesystem::remove(*target, error);
        return importFailed("COULD NOT REGISTER: " + *key);
    }

    Log::Print("Imported " + *key, "AssetManager", LogType::LOG_SUCCESS);
    return id;
}

bool AssetManager::moveFile(AssetId id, std::string_view destination)
{
    if (!project)
    {
        return moveFailed("NO PROJECT");
    }

    if (!registry.isUsable())
    {
        return moveFailed("ASSET REGISTRY IS UNUSABLE");
    }

    const AssetRecord *record = registry.findById(id);
    if (!record)
    {
        return moveFailed("UNKNOWN ASSET");
    }

    if (record->missing)
    {
        return moveFailed("ASSET FILE IS MISSING: " + record->path);
    }

    const std::string oldKey = record->path;
    const AssetType type = record->type;

    auto key = normaliseResourcePath(destination);
    if (!key)
    {
        return moveFailed("BAD DESTINATION PATH: " + std::string(destination));
    }

    if (*key == oldKey)
    {
        return true;
    }

    if (getAssetType(*key) != type)
    {
        return moveFailed("A MOVE CANNOT CHANGE THE ASSET TYPE: " + *key);
    }

    if (isInHiddenFolder(*key))
    {
        return moveFailed("DESTINATION IS IN A HIDDEN FOLDER: " + *key);
    }

    auto from = project->resolveResourcePath(oldKey);
    auto to = project->resolveResourcePath(*key);
    if (!from || !to)
    {
        return moveFailed("PATH IS OUTSIDE THE PROJECT: " + *key);
    }

    std::error_code error;
    if (std::filesystem::exists(*to, error))
    {
        return moveFailed("DESTINATION ALREADY EXISTS: " + *key);
    }

    std::filesystem::create_directories(to->parent_path(), error);
    if (error)
    {
        return moveFailed("CANNOT CREATE FOLDER FOR: " + *key);
    }

    std::filesystem::rename(*from, *to, error);
    if (error)
    {
        return moveFailed("RENAME FAILED: " + error.message());
    }

    if (!registry.setPath(id, *key))
    {
        std::filesystem::rename(*to, *from, error);
        return moveFailed("COULD NOT UPDATE REGISTRY: " + *key);
    }

    modelTable.renameKey(oldKey, *key);
    audioClipTable.renameKey(oldKey, *key);

    Log::Print("Moved " + oldKey + " -> " + *key, "AssetManager", LogType::LOG_SUCCESS);
    return true;
}

bool AssetManager::deleteFile(AssetId id)
{
    if (!project)
    {
        return deleteFailed("NO PROJECT");
    }

    if (!registry.isUsable())
    {
        return deleteFailed("ASSET REGISTRY IS UNUSABLE");
    }

    const AssetRecord *record = registry.findById(id);
    if (!record)
    {
        return deleteFailed("UNKNOWN ASSET");
    }

    const std::string key = record->path;

    if (!record->missing)
    {
        collectUnusedAssets();

        if (modelTable.find(key) || audioClipTable.find(key))
        {
            return deleteFailed("ASSET IS STILL IN USE: " + key);
        }

        auto file = project->resolveResourcePath(key);
        if (!file || !Core::Platform::moveToTrash(*file))
        {
            return deleteFailed("COULD NOT MOVE TO THE RECYCLE BIN: " + key);
        }
    }

    registry.markMissing(id);
    if (!registry.forget(id))
    {
        return deleteFailed("FILE TRASHED BUT REGISTRY NOT UPDATED: " + key);
    }

    Log::Print("Deleted " + key, "AssetManager", LogType::LOG_SUCCESS);
    return true;
}

bool AssetManager::setAudioImportSettings(AssetId id, const AudioImportSettings &settings)
{
    const AssetRecord *record = registry.findById(id);
    if (!record || record->type != AssetType::Audio)
    {
        return settingsFailed("NOT AN AUDIO ASSET");
    }

    if (record->audio == settings)
    {
        return true;
    }

    const std::string key = record->path;

    Core::AudioClipData *rebuilt = nullptr;
    const auto slot = audioClipTable.find(key);
    if (slot)
    {
        auto file = project ? project->resolveResourcePath(key) : std::nullopt;
        rebuilt = file ? Core::Audio::loadClip(file->string(), settings.stream) : nullptr;
        if (!rebuilt)
        {
            return settingsFailed("COULD NOT REIMPORT: " + key);
        }
    }

    if (!registry.setAudioImportSettings(id, settings))
    {
        Core::Audio::destroyClip(rebuilt);
        return settingsFailed("COULD NOT SAVE: " + key);
    }

    if (slot)
    {
        Core::AudioClipData *&current = audioClips[slot->index];
        Core::Audio::stopClipSounds(current);
        Core::Audio::destroyClip(current);
        current = rebuilt;

        Log::Print("Reimported " + key + (settings.stream ? " (streamed)" : ""), "AssetManager", LogType::LOG_SUCCESS);
    }

    return true;
}

} // namespace Cthulhu::Assets