#include "contentBrowserPanel.hpp"

#include <filesystem>
#include <limits>
#include <cassert>

#include "assetRegistry.hpp"
#include "project.hpp"
#include "session.hpp"
#include "engine.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
bool ContentBrowserPanel::beginMove(Context &ctx, const ContentBrowserEntry &entry)
{
    std::vector<std::string> folders;
    for (const auto &folder : index.entries())
    {
        if (!folder.folder || folder.missing || folder.path == entry.parent())
            continue;
        if (folders.size() ==
            static_cast<std::size_t>(std::numeric_limits<i32>::max()))
            return false;
        folders.push_back(folder.path);
    }
    if (folders.empty()) return false;

    ctx.setComboBoxItems(moveDestination, folders);
    ctx.setComboBoxSelectedIndex(moveDestination, 0);
    sourcePath = entry.path;
    pendingAsset = entry.assetId;
    pendingAction = PendingAction::Move;

    auto layout = ctx.layoutStyle(actionBar);
    layout.minSize = Vec2{0.0f, 60.0f};
    layout.preferredSize = Vec2{0.0f, 60.0f};
    layout.maxSize = Vec2{InfiniteLayoutSize, 60.0f};
    ctx.setVisible(actionLabel, false);
    ctx.setVisible(moveRow, true);
    ctx.setText(actionConfirm, "Move");
    ctx.setTooltip(actionConfirm,"Asset IDs are preserved. Path-only references in other scenes may need repair.");
    ctx.setVisible(actionBar, true);
    ctx.setFocused(actionCancel);
    return true;
}

Scribe::Result ContentBrowserPanel::moveAssetToFolder(Context &ctx, const Project::Project &project, const Assets::AssetRegistry &registry, Scribe::Session &session,
                                                     const Assets::AssetRecord &asset, std::string_view folder)
{
    const auto folderIndex = index.findFolder(folder);
    if (!folderIndex || index.entries()[*folderIndex].missing)
        return Scribe::Result::failed("Choose an existing destination folder.");

    const auto &folderPath = index.entries()[*folderIndex].path;
    assert(folderPath.ends_with('/'));

    const auto slash = asset.path.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= asset.path.size())
        return Scribe::Result::failed("Invalid source asset path.");

    if (std::string_view(asset.path).substr(0, slash + 1) == folderPath)
        return Scribe::Result::noChange();

    std::error_code error;
    const auto directory = project.resolveResourcePath(folderPath);
    if (!directory || !std::filesystem::is_directory(*directory, error) || error)
        return Scribe::Result::failed("The destination folder is unavailable. Refresh and try again.");

    const std::string destination = folderPath + asset.path.substr(slash + 1);
    const auto file = project.resolveResourcePath(destination);
    if (!file)
        return Scribe::Result::failed("The destination is outside the project.");

    if (registry.findByPath(destination))
        return Scribe::Result::failed("An asset with this name is already registered there.");

    const bool exists = std::filesystem::exists(*file, error);
    if (error)
        return Scribe::Result::failed("Could not check the destination: " + error.message());
    if (exists)
        return Scribe::Result::failed("A file with this name already exists in that folder.");

    const auto id = asset.id;
    auto result = session.moveAsset(id, destination);
    if (result.status == Scribe::Result::Status::Applied)
    {
        ctx.setText(searchInput, "");
        searchText.clear();
        appliedType = 0;
        ctx.setComboBoxSelectedIndex(typeFilter, 0);
        showFolder(ctx, folderPath);
        selectedAsset = id;
    }
    return result;
}

void ContentBrowserPanel::clearFolderDropHighlight(Context &ctx)
{
    if (highlightedDropFolder)
        ctx.setTreeViewItemHighlighted(folderTree, highlightedDropFolder, false);
    highlightedDropFolder = {};
}

std::optional<AssetDropFeedback> ContentBrowserPanel::folderDrop(Context &ctx, Engine &engine, Scribe::Session &session, const Assets::AssetRecord &asset,
                                                                bool released,
                                                                DrawList &feedback)
{
    const auto *project = engine.getProject();
    if (!project || !bound || boundProject != project->getProjectFilePath())
        return std::nullopt;

    const auto pointer = ctx.input().mouse.position;
    const auto entries = index.entries();
    const ContentBrowserEntry *folder = nullptr;
    TreeViewItemId row{};
    Rect tileRect{};

    if (ctx.isHovered(folderTree) && ctx.visibleRect(folderTree).contains(pointer))
    {
        const auto *state = ctx.tree().treeViewState(folderTree);
        const auto item = state ? state->hovered : TreeViewItemId{};
        if (ctx.treeViewItemExists(folderTree, item))
        {
            const auto entry = ctx.treeViewItemUserValue(folderTree, item);
            if (entry < entries.size() && entry < folderItems.size() && folderItems[entry] == item)
            {
                folder = &entries[entry];
                row = item;
            }
        }
    }
    else
    {
        for (const Tile &tile : tiles)
        {
            if (tile.entry >= entries.size() || !entries[tile.entry].folder || !ctx.isHovered(tile.button))
                continue;

            tileRect = ctx.visibleRect(tile.button);
            if (tileRect.contains(pointer))
                folder = &entries[tile.entry];
            break;
        }
    }

    if (!folder)
        return std::nullopt;
    if (folder->missing)
        return AssetDropFeedback{false, "The destination folder is unavailable.", released};

    const std::string_view path = asset.path;
    const auto slash = path.find_last_of('/');
    if (slash != std::string_view::npos && path.substr(0, slash + 1) == folder->path)
        return AssetDropFeedback{false, "Already in this folder."};

    if (!released)
    {
        if (row)
        {
            ctx.setTreeViewItemHighlighted(folderTree, row, true);
            highlightedDropFolder = row;
        }
        else
        {
            feedback.addRectOutline(tileRect.shrink(1.0f), ctx.theme().accent, 2.0f);
        }

        const auto name = folder->path == "res://" ? std::string_view{"project root"} : folder->name();
        return AssetDropFeedback{true, "Release to move into " + std::string(name) + "."};
    }

    const auto &registry = engine.getAssetManager().getRegistry();
    const auto result = moveAssetToFolder(ctx, *project, registry, session, asset, folder->path);
    if (!result.ok())
        return AssetDropFeedback{false, result.error, true};

    return AssetDropFeedback{true, result.status == Scribe::Result::Status::Applied ? "Asset moved." : "Already in this folder."};
}
} // namespace Cthulhu::Editor