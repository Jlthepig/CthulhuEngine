#include "contentBrowserIndex.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "assetRegistry.hpp"
#include "project.hpp"
#include "pathEncoding.hpp"

namespace Cthulhu::Editor
{
namespace
{
constexpr std::string_view ResourceRoot = "res://";

std::string foldAscii(std::string_view text)
{
    std::string result(text);
    for (char &character : result)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character + ('a' - 'A'));
        }
    }
    return result;
}

bool visibleResourcePath(std::string_view path)
{
    if (!path.starts_with(ResourceRoot) ||
        path.find('\\') != std::string_view::npos)
    {
        return false;
    }

    path.remove_prefix(ResourceRoot.size());
    while (!path.empty())
    {
        const std::size_t slash = path.find('/');
        const std::string_view part = path.substr(0, slash);
        if (part.empty() || part.front() == '.')
        {
            return false;
        }
        if (slash == std::string_view::npos)
        {
            break;
        }
        path.remove_prefix(slash + 1);
    }
    return true;
}

void sortUnique(std::vector<std::string> &paths)
{
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
}
} // namespace

std::string_view ContentBrowserEntry::name() const noexcept
{
    std::string_view value(path);
    if (value == ResourceRoot)
    {
        return value;
    }
    if (folder && !value.empty() && value.back() == '/')
    {
        value.remove_suffix(1);
    }

    const std::size_t slash = value.find_last_of('/');
    return slash == std::string_view::npos ? value : value.substr(slash + 1);
}

std::string_view ContentBrowserEntry::parent() const noexcept
{
    std::string_view value(path);
    if (value == ResourceRoot)
    {
        return {};
    }
    if (folder && !value.empty() && value.back() == '/')
    {
        value.remove_suffix(1);
    }

    const std::size_t slash = value.find_last_of('/');
    return slash == std::string_view::npos ? std::string_view{} : value.substr(0, slash + 1);
}

bool ContentBrowserIndex::rebuild(const Project::Project &project, const Assets::AssetRegistry &registry, std::string &error)
{
    error.clear();
    if (!registry.isUsable())
    {
        error = "The asset registry is unavailable.";
        return false;
    }

    try
    {
        std::vector<std::string> diskFolders{std::string(ResourceRoot)};
        const std::filesystem::path &root = project.getRootPath();

        std::error_code scanError;
        std::filesystem::recursive_directory_iterator iterator(
            root, std::filesystem::directory_options::none, scanError);
        const std::filesystem::recursive_directory_iterator end;

        while (!scanError && iterator != end)
        {
            const std::filesystem::directory_entry &entry = *iterator;
            const std::string filename = Utils::pathToUtf8(entry.path().filename());
            const auto status = entry.symlink_status(scanError);
            if (scanError)
            {
                break;
            }

            if ((!filename.empty() && filename.front() == '.') || std::filesystem::is_symlink(status))
            {
                iterator.disable_recursion_pending();
            }
            else if (std::filesystem::is_directory(status))
            {
                std::string path = std::string(ResourceRoot) + Utils::pathToUtf8(entry.path().lexically_relative(root)) + "/";

                // Uses the project's existing containment and file link policy
                if (visibleResourcePath(path) &&
                    project.resolveResourcePath(path))
                {
                    diskFolders.push_back(std::move(path));
                }
                else
                {
                    iterator.disable_recursion_pending();
                }
            }

            iterator.increment(scanError);
        }

        if (scanError)
        {
            error = "Could not scan project folders: " + scanError.message();
            return false;
        }

        sortUnique(diskFolders);
        std::vector<std::string> folders = diskFolders;
        std::vector<ContentBrowserEntry> assets;
        assets.reserve(registry.getRecords().size());

        for (const Assets::AssetRecord &record : registry.getRecords())
        {
            if (!record.id.isValid() ||
                record.type == Assets::AssetType::Unknown ||
                record.path.size() <= ResourceRoot.size() ||
                record.path.back() == '/' ||
                !visibleResourcePath(record.path) ||
                !project.resolveResourcePath(record.path))
            {
                continue;
            }

            ContentBrowserEntry asset{
                record.path, record.id, record.type, false, record.missing};

            // Keep missing assets reachable when their folders were removed
            std::string directory(asset.parent());
            while (directory.size() > ResourceRoot.size())
            {
                if (std::binary_search(
                        diskFolders.begin(), diskFolders.end(), directory))
                {
                    break;
                }

                folders.push_back(directory);
                directory.pop_back();

                const std::size_t slash = directory.find_last_of('/');
                if (slash == std::string::npos)
                {
                    break;
                }
                directory.resize(slash + 1);
            }

            assets.push_back(std::move(asset));
        }

        sortUnique(folders);
        std::sort(
            assets.begin(), assets.end(),
            [](const ContentBrowserEntry &left,
               const ContentBrowserEntry &right) {
                return left.path < right.path;
            });

        std::vector<ContentBrowserEntry> next;
        next.reserve(folders.size() + assets.size());

        for (const std::string &path : folders)
        {
            next.push_back(ContentBrowserEntry{
                path, {}, Assets::AssetType::Unknown, true,
                !std::binary_search(
                    diskFolders.begin(), diskFolders.end(), path)});
        }

        for (ContentBrowserEntry &asset : assets)
        {
            next.push_back(std::move(asset));
        }

        std::vector<std::string> nextSearchKeys;
        nextSearchKeys.reserve(next.size());
        for (const ContentBrowserEntry &entry : next)
        {
            nextSearchKeys.push_back(
                foldAscii(
                    std::string_view(entry.path).substr(ResourceRoot.size())));
        }

        // Commit only after the complete replacement has been built
        cached.swap(next);
        searchKeys.swap(nextSearchKeys);
        folderCount = folders.size();
        return true;
    }
    catch (const std::filesystem::filesystem_error &failure)
    {
        error = "Could not read project folders: " +
                std::string(failure.what());
        return false;
    }
}

std::optional<std::size_t>
ContentBrowserIndex::findFolder(std::string_view path) const
{
    const auto folders = entries().first(folderCount);
    const auto found = std::lower_bound(
        folders.begin(), folders.end(), path,
        [](const ContentBrowserEntry &entry, std::string_view wanted) {
            return std::string_view(entry.path) < wanted;
        });

    if (found == folders.end() || found->path != path)
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - folders.begin());
}

bool ContentBrowserIndex::query(std::string_view directory, std::string_view search, std::optional<Assets::AssetType> type, std::vector<std::size_t> &out) const
{
    out.clear();
    if (!findFolder(directory))
    {
        return false;
    }

    const std::size_t first = search.find_first_not_of(" \t\r\n");
    const std::size_t last = search.find_last_not_of(" \t\r\n");
    const std::string needle = first == std::string_view::npos
        ? std::string{}
        : foldAscii(search.substr(first, last - first + 1));
    const bool recursive = !needle.empty() || type.has_value();

    for (std::size_t index = 0; index < cached.size(); ++index)
    {
        const ContentBrowserEntry &entry = cached[index];
        if (!recursive)
        {
            if (entry.parent() == directory)
            {
                out.push_back(index);
            }
            continue;
        }

        if (entry.folder ||
            !entry.path.starts_with(directory) ||
            (type && entry.type != *type) ||
            (!needle.empty() &&
             searchKeys[index].find(needle) == std::string::npos))
        {
            continue;
        }

        out.push_back(index);
    }
    return true;
}
} // namespace Cthulhu::Editor