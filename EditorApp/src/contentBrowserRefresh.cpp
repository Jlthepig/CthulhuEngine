#include "contentBrowserPanel.hpp"

#include <algorithm>
#include <utility>

#include "project.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
void ContentBrowserPanel::reloadCatalog(Context &ctx, const Project::Project &project, const Assets::AssetRegistry &registry)
{
    constexpr std::string_view Root = "res://";
    const float scroll = ctx.scrollOffsetY(assetScroll);

    std::string error;
    if (!index.rebuild(project, registry, error))
    {
        ctx.setText(errorLabel, error);
        ctx.setTooltip(errorLabel, error);
        ctx.setVisible(errorLabel, true);
        return;
    }

    const Assets::AssetId asset = selectedAsset;
    const std::string folderSelection = selectedFolder;
    std::string destination = currentFolder;

    while (!index.findFolder(destination))
    {
        if (destination.size() <= Root.size())
        {
            destination = Root;
            break;
        }

        destination.pop_back();
        const std::size_t slash = destination.find_last_of('/');
        if (slash == std::string::npos)
        {
            destination = Root;
            break;
        }
        destination.resize(slash + 1);
    }

    const bool sameFolder = destination == currentFolder;

    // Keep the surviving history entries and the current fallback in their order
    std::size_t write{};
    std::size_t nextCursor{};
    for (std::size_t read = 0; read < history.size(); ++read)
    {
        if (read == historyCursor)
        {
            history[read] = destination;
            nextCursor = write;
        }
        else if (!index.findFolder(history[read]))
        {
            continue;
        }

        if (write != read) history[write] = std::move(history[read]);
        ++write;
    }

    history.resize(write);
    if (history.empty()) history.push_back(destination);
    historyCursor = nextCursor;

    // Old tile/tree indices belong to the previous catalog
    clearTiles(ctx);
    ctx.clearTreeView(folderTree);
    rebuildTree(ctx, project);

    searchText.assign(ctx.text(searchInput));
    appliedType = std::clamp(ctx.comboBoxSelectedIndex(typeFilter), 0, 3);
    showFolder(ctx, destination, false);
    if (sameFolder) ctx.setScrollOffsetY(assetScroll, scroll);

    const auto entries = index.entries();
    for (std::size_t entry : visible)
    {
        const auto &value = entries[entry];
        if ((!value.folder && asset.isValid() && value.assetId == asset) || (value.folder && !folderSelection.empty() && value.path == folderSelection))
        {
            select(ctx, entry);
            break;
        }
    }

    ctx.setText(errorLabel, "");
    ctx.setTooltip(errorLabel, "");
    ctx.setVisible(errorLabel, false);
}
} // namespace Cthulhu::Editor