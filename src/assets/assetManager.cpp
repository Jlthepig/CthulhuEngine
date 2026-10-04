#include <algorithm>
#include <filesystem>

#include "assetManager.hpp"
#include "audio.hpp"
#include "modelLoader.hpp"
#include "project.hpp"
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

} // namespace Cthulhu::Assets