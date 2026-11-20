#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "project.hpp"
#include "projectParser.hpp"
#include "projectWriter.hpp"
#include "pathEncoding.hpp"
#include "assetRegistry.hpp"
#include "log_utils.hpp"

using namespace KalaHeaders::KalaLog;

namespace Cthulhu::Project
{

static bool hasDirectResourcePath(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    std::filesystem::path current = root;

    for (const auto &part : relative.lexically_normal())
    {
        current /= part;

#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            const DWORD error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        }

        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            return false;
        }
#else
        std::error_code error;
        const auto status = std::filesystem::symlink_status(current, error);
        if (error == std::errc::no_such_file_or_directory)
        {
            return true;
        }
        if (error || std::filesystem::is_symlink(status))
        {
            return false;
        }
#endif
    }

    return true;
}

static bool isInsideProjectRoot(const std::filesystem::path &root, const std::filesystem::path &path)
{
    auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());

    if (relative.empty())
        return false;

    return *relative.begin() != "..";
}

std::optional<std::filesystem::path> Project::resolveResourcePath(std::string_view resourcePath) const
{
    constexpr std::string_view prefix = "res://";

    if (!resourcePath.starts_with(prefix))
    {
        Log::Print("RESOURCE PATH MUST START WITH 'res://': " + std::string(resourcePath), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    std::string_view relativePart = resourcePath.substr(prefix.size());

    const auto decoded = Utils::pathFromUtf8(relativePart);
    if (!decoded) return std::nullopt;

    const std::filesystem::path &relativePath = *decoded;

    if (relativePath.is_absolute() || relativePath.has_root_name() || relativePath.has_root_directory())
    {
        Log::Print("INVALID RESOURCE PATH: " + std::string(resourcePath), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    std::error_code error;
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(rootPath, error);
    if (error)
    {
        Log::Print("FAILED TO RESOLVE PROJECT ROOT: " + rootPath.string(), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    const std::filesystem::path resolved = std::filesystem::weakly_canonical(rootPath / relativePath, error);
    if (error)
    {
        Log::Print("FAILED TO RESOLVE RESOURCE PATH: " + std::string(resourcePath), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    if (!isInsideProjectRoot(canonicalRoot, resolved))
    {
        Log::Print("RESOURCE PATH ESCAPES PROJECT ROOT: " + std::string(resourcePath), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    if (!hasDirectResourcePath(canonicalRoot, relativePath))
    {
        Log::Print("RESOURCE PATH CONTAINS A LINK OR CANNOT BE CHECKED: " + std::string(resourcePath),"Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    return resolved;
}

bool Project::setMainScene(std::optional<Assets::AssetId> id)
{
    if (id && !id->isValid())
        return false;

    if (!config.mainScene && config.mainSceneId == id)
        return true;

    auto updated = config;
    updated.mainScene.reset();
    updated.mainSceneId = id;
    return saveConfig(std::move(updated));
}

bool Project::saveConfig(ProjectConfig updated)
{
    if (!ProjectWriter::write(projectFilePath, updated))
        return false;

    config = std::move(updated);
    return true;
}

std::string_view Project::setDetails(std::string_view name, uint32_t width, uint32_t height)
{
    if (name.find_first_not_of(' ') == std::string_view::npos)
        return "Enter a project name.";

    for (const unsigned char character : name)
        if (character < 32 || character == 127)
            return "The project name must be a single line of text.";

    const auto maxSize = ProjectParser::getMaxWindowDimension();
    if (width == 0 || height == 0 || width > maxSize || height > maxSize)
        return "Enter a width and height between 1 and 16384.";

    if (config.name == name && config.windowWidth == width && config.windowHeight == height)
        return {};

    auto updated = config;
    updated.name = name;
    updated.windowWidth = width;
    updated.windowHeight = height;
    if (!saveConfig(std::move(updated)))
        return "Could not save project settings. See the log.";

    return {};
}

const std::string *Project::getMainScene(const Assets::AssetRegistry &registry) const
{
    if (!config.mainSceneId)
        return config.mainScene ? &*config.mainScene : nullptr;

    const auto *record = registry.findById(*config.mainSceneId);
    return record && record->type == Assets::AssetType::Scene && !record->missing ? &record->path : nullptr;
}

bool Project::isMainScene(const Assets::AssetRecord &record, const Assets::AssetRegistry &registry) const
{
    if (record.type != Assets::AssetType::Scene)
        return false;
    if (config.mainSceneId == record.id)
        return true;

    const auto *path = getMainScene(registry);
    if (!path)
        return false;
    if (record.path == *path)
        return true;

    const auto assigned = resolveResourcePath(*path);
    const auto candidate = resolveResourcePath(record.path);
    if (!assigned || !candidate)
        return false;
    if (*assigned == *candidate)
        return true;

    std::error_code error;
    return std::filesystem::equivalent(*assigned, *candidate, error) && !error;
}

std::optional<Project> Project::createProject(const std::filesystem::path &requestedRoot, const ProjectConfig &config)
{
    if (config.name.empty())
    {
        Log::Print("PROJECT NAME CANNOT BE EMPTY", "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    uint32_t maxWindowDimension = ProjectParser::getMaxWindowDimension();

    if (config.windowWidth == 0 || config.windowHeight == 0 || config.windowWidth > maxWindowDimension ||
        config.windowHeight > maxWindowDimension)
    {
        Log::Print("INVALID WINDOW DIMENSIONS", "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    std::error_code error;

    auto root = std::filesystem::absolute(requestedRoot, error);

    if (error)
    {
        Log::Print("FAILED TO RESOLVE PROJECT DIRECTOY: " + requestedRoot.string(), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    root = root.lexically_normal();

    const bool rootAlreadyExisted = std::filesystem::exists(root);

    auto rollback = [&]() {
        std::error_code cleanupError;

        std::filesystem::remove(root / "project.cthulhu", cleanupError);

        std::filesystem::remove_all(root / ".cthulhu", cleanupError);

        if (!rootAlreadyExisted)
        {
            std::filesystem::remove(root, cleanupError);
        }
    };

    if (std::filesystem::exists(root, error))
    {
        if (error || !std::filesystem::is_directory(root))
        {
            Log::Print("PROJECT DESTINATION IS NOT A VALID DIRECTORY: " + root.string(), "Project", LogType::LOG_ERROR);
            return std::nullopt;
        }

        if (!std::filesystem::is_empty(root, error) || error)
        {
            Log::Print("PROJECT DIRECTORY MUST BE EMPTY: " + root.string(), "Project", LogType::LOG_ERROR);
            return std::nullopt;
        }
    }
    else
    {
        if (!std::filesystem::create_directories(root, error) || error)
        {
            Log::Print("FAILED TO CREATE PROJECT DIRECTORY: " + root.string(), "Project", LogType::LOG_ERROR);
            return std::nullopt;
        }
    }

    const auto internalDirectoy = root / ".cthulhu";

    if (!std::filesystem::create_directories(internalDirectoy, error) || error)
    {
        Log::Print("FAILED TO CREATE .cthulhu DIRECTORY", "Project", LogType::LOG_ERROR);
        rollback();
        return std::nullopt;
    }

    const auto projectFile = root / "project.cthulhu";

    if (!ProjectWriter::write(projectFile, config))
    {
        rollback();
        return std::nullopt;
    }

    auto project = Project::open(projectFile);

    if (!project)
    {
        Log::Print("PROJECT WAS CREATED BUT CANNOT BE OPENED", "Project", LogType::LOG_ERROR);
        rollback();
        return std::nullopt;
    }

    Log::Print("PROJECT WAS CREATED SUCCESSFULLY: " + config.name, "Project", LogType::LOG_SUCCESS);
    return project;
}

std::optional<Project> Project::open(const std::filesystem::path &projectFilePath)
{
    std::error_code error;

    std::filesystem::path absolutePath = std::filesystem::absolute(projectFilePath, error);

    if (error)
    {
        Log::Print("FAILED TO RESOLVE PROJECT PATH: " + projectFilePath.string(), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    absolutePath = absolutePath.lexically_normal();

    std::filesystem::path canonicalProjectFile = std::filesystem::canonical(absolutePath, error);

    if (error)
    {
        Log::Print("FAILED TO CANONICALIZE PROJECT PATH: " + absolutePath.string(), "Project", LogType::LOG_ERROR);

        return std::nullopt;
    }

    absolutePath = std::move(canonicalProjectFile);

    if (absolutePath.filename() != "project.cthulhu")
    {
        Log::Print("PROJECT FILE MUST BE NAMED 'project.cthulhu'", "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    if (!std::filesystem::is_regular_file(absolutePath, error) || error)
    {
        Log::Print("PROJECT FILE DOES NOT EXIST: " + absolutePath.string(), "Project", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto parsedConfig = ProjectParser::parse(Utils::pathToUtf8(absolutePath));

    if (!parsedConfig)
    {
        return std::nullopt;
    }

    Project project;
    project.config = std::move(*parsedConfig);
    project.projectFilePath = absolutePath;
    project.rootPath = absolutePath.parent_path();

    const auto internalDirectory = project.rootPath / ".cthulhu";

    std::filesystem::create_directories(internalDirectory, error);

    if (error)
    {
        Log::Print("FAILED TO CREATE PROJECT INTERNAL DIRECTORY: " + internalDirectory.string(),
                   "Project",
                   LogType::LOG_ERROR);
        return std::nullopt;
    }

    if (project.config.mainScene)
    {
        if (!project.resolveResourcePath(*project.config.mainScene))
        {
            Log::Print("INVALID MAIN SCENE RESOURCE PATH: " + *project.config.mainScene, "Project", LogType::LOG_ERROR);
            return std::nullopt;
        }
    }

    Log::Print("PROJECT OPENED: " + project.config.name, "Project", LogType::LOG_SUCCESS);
    return project;
}
} // namespace Cthulhu::Project
