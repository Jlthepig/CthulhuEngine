#include <algorithm>
#include <filesystem>

#include "assetManager.hpp"
#include "assetType.hpp"
#include "modelLoader.hpp"
#include "project.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Assets
{
std::optional<std::string> normalizeResourcePath(std::string_view resourcePath)
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
    if (modelTable.liveCount() != 0)
    {
        Log::Print("ASSET MANAGER DESTROYED WITHOUT SHUTDOWN", "AssetManager", LogType::LOG_WARNING);
    }
}

ModelHandle AssetManager::loadModel(std::string_view resourcePath)
{
    auto key = normalizeResourcePath(resourcePath);
    if (!key)
    {
        Log::Print("INVALID MODEL RESOURCE PATH: " + std::string(resourcePath), "AssetManager", LogType::LOG_ERROR);
        return {};
    }

    if (const AssetType type = getAssetType(*key); type != AssetType::Model)
    {
        Log::Print("NOT A MODEL ASSET: " + *key + " (type: " + std::string(assetTypeName(type)) + ")",
                   "AssetManager", LogType::LOG_ERROR);
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
    project = nullptr;
}
} // namespace Cthulhu::Assets