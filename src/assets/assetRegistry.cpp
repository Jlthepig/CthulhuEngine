#include <algorithm>
#include <fstream>
#include <system_error>

#include <simdjson.h>

#include "assetManager.hpp"
#include "assetRegistry.hpp"
#include "project.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Assets
{
namespace
{
constexpr int64_t REGISTRY_FORMAT_VERSION = 1;

std::string escapeJson(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            if (static_cast<unsigned char>(c) >= 0x20)
            {
                out += c;
            }
        }
    }
    return out;
}
} // namespace

bool AssetRegistry::load(const std::filesystem::path &registryFile)
{
    clear();
    file = registryFile;

    auto fail = [&](const std::string &message) {
        Log::Print(message + " (" + file.string() + ")", "AssetRegistry", LogType::LOG_ERROR);
        records.clear();
        indexById.clear();
        indexByPath.clear();
        usable = false;
        return false;
    };

    std::error_code error;
    if (!std::filesystem::exists(file, error))
    {
        usable = true; // new project should start empty
        return true;
    }

    simdjson::ondemand::parser parser;
    auto json = simdjson::padded_string::load(file.string());
    if (json.error())
    {
        return fail("FAILED TO READ ASSET REGISTRY");
    }

    auto doc = parser.iterate(json);

    auto version = doc["format_version"].get_int64();
    if (version.error() || version.value() != REGISTRY_FORMAT_VERSION)
    {
        return fail("ASSET REGISTRY HAS MISSING OR UNSUPPORTED format_version");
    }

    auto assets = doc["assets"].get_array();
    if (assets.error())
    {
        return fail("ASSET REGISTRY IS MISSING 'assets'");
    }

    for (auto entry : assets.value())
    {
        auto object = entry.get_object();
        if (object.error())
        {
            return fail("INVALID ASSET REGISTRY ENTRY");
        }

        auto idText = object.value()["id"].get_string();
        if (idText.error())
        {
            return fail("ASSET REGISTRY ENTRY IS MISSING 'id'");
        }
        const auto id = assetIdFromString(idText.value());

        auto pathText = object.value()["path"].get_string();
        if (!id || pathText.error())
        {
            return fail("ASSET REGISTRY ENTRY HAS INVALID 'id' OR 'path'");
        }

        auto path = normaliseResourcePath(pathText.value());
        if (!path)
        {
            return fail("ASSET REGISTRY ENTRY HAS INVALID PATH");
        }

        if (indexById.contains(*id) || indexByPath.contains(*path))
        {
            return fail("ASSET REGISTRY CONTAINS DUPLICATE ID OR PATH: " + *path);
        }

        add(*id, std::move(*path));

        auto import = object.value()["import"].get_object();
        if (!import.error())
        {
            auto stream = import.value()["stream"].get_bool();
            if (!stream.error())
            {
                records.back().audio.stream = stream.value();
            }
        }
    }

    usable = true;
    return true;
}

bool AssetRegistry::save() const
{
    if (!usable || file.empty())
    {
        Log::Print("REFUSING TO SAVE UNUSABLE ASSET REGISTRY", "AssetRegistry", LogType::LOG_ERROR);
        return false;
    }

    std::vector<const AssetRecord *> sorted;
    sorted.reserve(records.size());
    for (const auto &record : records)
    {
        sorted.push_back(&record);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const AssetRecord *a, const AssetRecord *b) { return a->path < b->path; });

    // preventing a mid write crash to corrupt registry
    const auto temp = file.parent_path() / (file.filename().string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out.is_open())
        {
            Log::Print("FAILED TO WRITE ASSET REGISTRY: " + temp.string(), "AssetRegistry", LogType::LOG_ERROR);
            return false;
        }

        out << "{\n    \"format_version\": " << REGISTRY_FORMAT_VERSION << ",\n    \"assets\": [";
        for (std::size_t i = 0; i < sorted.size(); ++i)
        {
            const AssetRecord &record = *sorted[i];

            out << (i == 0 ? "\n" : ",\n") << "        { \"id\": \"" << assetIdToString(record.id)
                << "\", \"path\": \"" << escapeJson(record.path) << "\"";

            if (record.type == AssetType::Audio && record.audio != AudioImportSettings{})
            {
                out << ", \"import\": { \"stream\": " << (record.audio.stream ? "true" : "false") << " }";
            }

            out << " }";
        }
        out << (sorted.empty() ? "]\n}\n" : "\n    ]\n}\n");

        out.flush();
        if (!out.good())
        {
            Log::Print("FAILED WHILE WRITING ASSET REGISTRY", "AssetRegistry", LogType::LOG_ERROR);
            return false;
        }
    }

    std::error_code error;
    std::filesystem::rename(temp, file, error);
    if (error)
    {
        Log::Print("FAILED TO REPLACE ASSET REGISTRY: " + file.string(), "AssetRegistry", LogType::LOG_ERROR);
        std::filesystem::remove(temp, error);
        return false;
    }

    return true;
}

AssetRegistry::ScanResult AssetRegistry::scan(const Project::Project &project)
{
    ScanResult result;

    if (!usable)
    {
        Log::Print("ASSET REGISTRY IS UNUSABLE, SCAN SKIPPED. FIX OR DELETE .cthulhu/assets.json", "AssetRegistry",
                   LogType::LOG_ERROR);
        return result;
    }

    const auto &root = project.getRootPath();
    std::vector<bool> seen(records.size(), false);

    std::error_code error;
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, error);

    if (error)
    {
        Log::Print("FAILED TO SCAN PROJECT: " + root.string(), "AssetRegistry", LogType::LOG_ERROR);
        return result;
    }

    for (const std::filesystem::recursive_directory_iterator end; it != end; it.increment(error))
    {
        if (error)
        {
            Log::Print("PROJECT SCAN STOPPED EARLY: " + error.message(), "AssetRegistry", LogType::LOG_WARNING);
            break;
        }

        const auto &entry = *it;
        const std::string name = entry.path().filename().string();

        if (entry.is_directory(error))
        {
            // skip nonsense folders like .vs etc.
            if (!name.empty() && name.front() == '.')
            {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!entry.is_regular_file(error))
        {
            continue;
        }

        auto key = normaliseResourcePath("res://" + entry.path().lexically_relative(root).generic_string());
        if (!key || getAssetType(*key) == AssetType::Unknown)
        {
            continue;
        }

        if (auto found = indexByPath.find(*key); found != indexByPath.end())
        {
            seen[found->second] = true;
            continue;
        }

        AssetId id = generateAssetId();
        while (indexById.contains(id))
        {
            id = generateAssetId();
        }

        add(id, std::move(*key));
        seen.push_back(true);
        ++result.added;
    }

    for (std::size_t i = 0; i < records.size(); ++i)
    {
        records[i].missing = !seen[i];
        if (records[i].missing)
        {
            ++result.missing;
        }
    }

    result.total = records.size();
    result.ok = result.added == 0 || save();

    Log::Print("Asset registry: " + std::to_string(result.total) + " assets (" + std::to_string(result.added) +
                   " new, " + std::to_string(result.missing) + " missing)",
               "AssetRegistry", LogType::LOG_INFO);
    return result;
}

bool AssetRegistry::setAudioImportSettings(AssetId id, const AudioImportSettings &settings)
{
    if (!usable)
    {
        return false;
    }

    auto it = indexById.find(id);
    if (it == indexById.end())
    {
        return false;
    }

    AssetRecord &record = records[it->second];
    if (record.type != AssetType::Audio)
    {
        Log::Print("NOT AN AUDIO ASSET: " + record.path, "AssetRegistry", LogType::LOG_ERROR);
        return false;
    }

    if (record.audio == settings)
    {
        return true;
    }

    record.audio = settings;
    return save();
}

bool AssetRegistry::forget(AssetId id)
{
    auto it = indexById.find(id);
    if (it == indexById.end())
    {
        return false;
    }

    if (!records[it->second].missing)
    {
        Log::Print("ONLY MISSING ASSETS CAN BE FORGOTTEN: " + records[it->second].path, "AssetRegistry",
                   LogType::LOG_ERROR);
        return false;
    }

    records.erase(records.begin() + static_cast<std::ptrdiff_t>(it->second));
    rebuildIndices();
    return save();
}

const AssetRecord *AssetRegistry::findById(AssetId id) const
{
    auto it = indexById.find(id);
    return it != indexById.end() ? &records[it->second] : nullptr;
}

const AssetRecord *AssetRegistry::findByPath(std::string_view resourcePath) const
{
    const auto key = normaliseResourcePath(resourcePath);
    if (!key)
    {
        return nullptr;
    }

    auto it = indexByPath.find(*key);
    return it != indexByPath.end() ? &records[it->second] : nullptr;
}

void AssetRegistry::clear()
{
    file.clear();
    usable = false;
    records.clear();
    indexById.clear();
    indexByPath.clear();
}

void AssetRegistry::add(AssetId id, std::string path)
{
    AssetRecord record;
    record.id = id;
    record.type = getAssetType(path);
    record.path = std::move(path);

    records.push_back(std::move(record));

    const std::size_t index = records.size() - 1;
    indexById.emplace(records[index].id, index);
    indexByPath.emplace(records[index].path, index);
}

void AssetRegistry::rebuildIndices()
{
    indexById.clear();
    indexByPath.clear();

    for (std::size_t i = 0; i < records.size(); ++i)
    {
        indexById.emplace(records[i].id, i);
        indexByPath.emplace(records[i].path, i);
    }
}
} // namespace Cthulhu::Assets