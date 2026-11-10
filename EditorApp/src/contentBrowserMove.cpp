#include "contentBrowserPanel.hpp"

#include <filesystem>
#include <limits>

#include "assetRegistry.hpp"
#include "project.hpp"
#include "session.hpp"

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

Scribe::Result ContentBrowserPanel::finishMove(Context &ctx, const Project::Project *project, const Assets::AssetRegistry &registry, Scribe::Session &session)
{
    const std::string folder(ctx.comboBoxSelectedText(moveDestination));
    const auto folderIndex = index.findFolder(folder);
    if (!folderIndex || index.entries()[*folderIndex].missing) return Scribe::Result::failed("Choose an existing destination folder.");

    std::error_code error;
    const auto directory = project->resolveResourcePath(folder);
    if (!directory || !std::filesystem::is_directory(*directory, error) || error)
    {
        return Scribe::Result::failed("The destination folder is unavailable. Refresh and try again.");
    }

    const std::size_t slash = sourcePath.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= sourcePath.size()) return Scribe::Result::failed("Invalid source asset path.");

    const std::string destination = folder + sourcePath.substr(slash + 1);
    const auto file = project->resolveResourcePath(destination);
    if (!file) return Scribe::Result::failed("The destination is outside the project.");
    if (registry.findByPath(destination))
    {
        return Scribe::Result::failed("An asset with this name is already registered there.");
    }

    const bool exists = std::filesystem::exists(*file, error);
    if (error)return Scribe::Result::failed("Could not check the destination: " + error.message());
    if (exists)
    {
        return Scribe::Result::failed("A file with this name already exists in that folder.");
    }

    const Assets::AssetId id = pendingAsset;
    auto result = session.moveAsset(id, destination);
    if (result.status == Scribe::Result::Status::Applied)
    {
        // keep the moved asset visible in its destination
        ctx.setText(searchInput, "");
        searchText.clear();
        appliedType = 0;
        ctx.setComboBoxSelectedIndex(typeFilter, 0);
        showFolder(ctx, folder);
        selectedAsset = id;
    }
    return result;
}
} // namespace Cthulhu::Editor