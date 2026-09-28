#include <algorithm>
#include <filesystem>

#include "assetManager.hpp"
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
    if (!modelSlots.empty())
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

    if (auto it = modelIndexByPath.find(*key); it != modelIndexByPath.end())
    {
        const uint32_t index = it->second;
        return ModelHandle{index, modelSlots[index].generation};
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

    const uint32_t index = static_cast<uint32_t>(modelSlots.size());

    ModelSlot slot;
    slot.model = std::make_unique<Rendering::Model>(std::move(*loaded));
    slot.resourcePath = *key;
    slot.generation = 1;

    modelSlots.push_back(std::move(slot));
    modelIndexByPath.emplace(*key, index);

    Log::Print("Loaded model: " + *key, "AssetManager", LogType::LOG_INFO);
    return ModelHandle{index, 1};
}

Rendering::Model *AssetManager::getModel(ModelHandle handle)
{
    if (!handle.isValid() || handle.index >= modelSlots.size())
    {
        return nullptr;
    }

    auto &slot = modelSlots[handle.index];
    if (slot.generation != handle.generation || !slot.model)
    {
        return nullptr;
    }

    return slot.model.get();
}

const Rendering::Model *AssetManager::getModel(ModelHandle handle) const
{
    if (!handle.isValid() || handle.index >= modelSlots.size())
    {
        return nullptr;
    }

    const auto &slot = modelSlots[handle.index];
    if (slot.generation != handle.generation || !slot.model)
    {
        return nullptr;
    }

    return slot.model.get();
}

ModelHandle AssetManager::acquireModel(std::string_view resourcePath)
{
    const ModelHandle handle = loadModel(resourcePath);
    if (handle.isValid())
    {
        ++modelSlots[handle.index].refCount;
    }
    return handle;
}

void AssetManager::releaseModel(ModelHandle handle)
{
    if (!handle.isValid() || handle.index >= modelSlots.size())
    {
        return;
    }

    auto &slot = modelSlots[handle.index];
    if (slot.generation != handle.generation)
    {
        return;
    }

    if (slot.refCount == 0)
    {
        Log::Print("MODEL RELEASED MORE TIMES THAN ACQUIRED: " + slot.resourcePath, "AssetManager", LogType::LOG_ERROR);
        return;
    }

    --slot.refCount;
}

uint32_t AssetManager::getModelRefCount(ModelHandle handle) const
{
    if (!handle.isValid() || handle.index >= modelSlots.size())
    {
        return 0;
    }

    const auto &slot = modelSlots[handle.index];
    return slot.generation == handle.generation ? slot.refCount : 0;
}

uint32_t AssetManager::getTotalModelRefCount() const noexcept
{
    uint32_t total = 0;
    for (const auto &slot : modelSlots)
    {
        total += slot.refCount;
    }
    return total;
}

std::size_t AssetManager::getLoadedModelCount() const noexcept
{
    return modelIndexByPath.size();
}

void AssetManager::shutdown()
{
    for (auto &slot : modelSlots)
    {
        if (slot.model)
        {
            slot.model->destroy();
        }
    }

    modelSlots.clear();
    modelIndexByPath.clear();
    project = nullptr;
}
} // namespace Cthulhu::Assets