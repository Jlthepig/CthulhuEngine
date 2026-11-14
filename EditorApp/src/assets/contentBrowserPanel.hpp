#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>

#include "OctoGui/Context.h"
#include "contentBrowserIndex.hpp"
#include "assetDrag.hpp"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Assets
{
struct AssetRecord;
}
namespace Cthulhu::Scribe
{
class Session;
struct Result;
}

namespace Cthulhu::Editor
{
class ContentBrowserPanel
{
  public:
    void build(octogui::Context &ctx, octogui::NodeHandle parent);
    void update(octogui::Context &ctx, Engine &engine, Scribe::Session &session, bool assetsChanged, bool dragOwnsInput);

    Assets::AssetId beginAssetDrag(octogui::Context &ctx, const Project::Project &project);
    void reportError(octogui::Context &ctx, std::string_view message);

    void clearFolderDropHighlight(octogui::Context &ctx);
    std::optional<AssetDropFeedback> folderDrop(octogui::Context &ctx, Engine &engine, Scribe::Session &session, const Assets::AssetRecord &asset, 
                                                bool released,
                                                octogui::DrawList &feedback);
    
    Assets::AssetId takeSceneOpenRequest();
    Assets::AssetId takeDeleteRequest();

  private:
    struct Crumb
    {
        octogui::NodeHandle button{};
        std::string path;
    };

    static std::string caption(octogui::Context &ctx,
                               std::string_view text, float width);
    void applyFilter(octogui::Context &ctx);
    void rebuildBreadcrumbs(octogui::Context &ctx);

    struct Tile
    {
        octogui::NodeHandle button{};
        std::size_t entry{};
    };

    void bindProject(octogui::Context &ctx, const Project::Project *project, const Assets::AssetRegistry &registry);
    void rebuildTree(octogui::Context &ctx, const Project::Project &project);
    void showFolder(octogui::Context &ctx, std::string_view path, bool recordHistory = true);
    void rebuildTiles(octogui::Context &ctx);
    void clearTiles(octogui::Context &ctx);
    void select(octogui::Context &ctx, std::size_t entry);
    void styleSelection(octogui::Context &ctx);

    void reloadCatalog(octogui::Context &ctx, const Project::Project &project, const Assets::AssetRegistry &registry);
    bool updateActions(octogui::Context &ctx, const Project::Project *project, const Assets::AssetRegistry &registry, Scribe::Session &session, bool canEdit);
    bool beginMove(octogui::Context &ctx, const ContentBrowserEntry &entry);
    Scribe::Result moveAssetToFolder(octogui::Context &ctx, const Project::Project &project, const Assets::AssetRegistry &registry, Scribe::Session &session,
                                    const Assets::AssetRecord &asset,
                                    std::string_view folder);
    void cancelAction(octogui::Context &ctx);

    void importAsset(octogui::Context &ctx, Engine &engine, Scribe::Session &session);

    octogui::NodeHandle toolbar{}, title{};
    octogui::NodeHandle importButton{}, searchInput{}, typeFilter{}, refreshButton{};
    octogui::NodeHandle backButton{}, forwardButton{}, upButton{};
    octogui::NodeHandle breadcrumbs{}, breadcrumbLabel{}, errorLabel{};
    octogui::NodeHandle folderTree{}, assetScroll{}, grid{}, emptyLabel{};
    octogui::NodeHandle selectionLabel{}, countLabel{};
    octogui::NodeHandle actionBar{}, actionConfirm{}, actionCancel{};
    octogui::NodeHandle moveRow{}, moveDestination{};

    Assets::AssetId pendingAsset{};
    std::string sourcePath;
    std::string actionError;

    ContentBrowserIndex index;
    std::filesystem::path boundProject;
    std::vector<octogui::TreeViewItemId> folderItems;
    octogui::TreeViewItemId highlightedDropFolder{};
    std::vector<std::size_t> visible;
    std::vector<octogui::NodeHandle> rows;
    std::vector<Tile> tiles;

    std::vector<std::string> history;
    std::size_t historyCursor{};
    std::string searchText;
    int appliedType{};

    std::vector<Crumb> crumbs;
    std::vector<octogui::NodeHandle> crumbNodes;
    std::vector<std::string> crumbPaths;
    std::vector<float> crumbWidths;
    std::size_t firstCrumb{};
    bool crumbsDirty = true;

    std::string currentFolder = "res://";
    Assets::AssetId selectedAsset{};
    Assets::AssetId sceneOpenRequest{};
    Assets::AssetId deleteRequest{};
    std::string selectedFolder;
    std::string lastFolderClick;
    float folderClickAge = 1.0f;
    std::size_t columns{};
    bool bound = false;
    bool tilesDirty = true;
    bool catalogPending = false;
};
} // namespace Cthulhu::Editor