#include "contentBrowserPanel.hpp"

#include <algorithm>
#include <cmath>

#include "assetRegistry.hpp"
#include "project.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
namespace
{
constexpr float TileWidth = 112.0f;
constexpr float TileHeight = 88.0f;
constexpr float TileGap = 8.0f;
constexpr float GridInset = 6.0f;

void fixed(Context &ctx, NodeHandle node, float width, float height)
{
    auto layout = ctx.layoutStyle(node);
    layout.minSize = Vec2{width, height};
    layout.preferredSize = Vec2{width, height};
    layout.maxSize = Vec2{width, height};
}

void fillPane(Context &ctx, NodeHandle pane)
{
    auto layout = ctx.layoutStyle(pane);
    layout.mode = LayoutMode::Stack;
    layout.horizontalAlignment = Alignment::Stretch;
    layout.verticalAlignment = Alignment::Stretch;
    layout.padding = Padding{};
    layout.gap = 0.0f;
    layout.clip = true;

    auto style = ctx.style(pane);
    style.all.background = Color::transparent();
    style.all.border = Color::transparent();
    style.shadow = ShadowStyle{};
}

NodeHandle bar(Context &ctx, NodeHandle parent, float height)
{
    const NodeHandle node = ctx.createContainer(parent);
    auto layout = ctx.layoutStyle(node);
    layout.mode = LayoutMode::Horizontal;
    layout.verticalAlignment = Alignment::Center;
    layout.padding = Padding{8.0f, 2.0f};
    layout.gap = 6.0f;
    layout.minSize = Vec2{0.0f, height};
    layout.preferredSize = Vec2{0.0f, height};
    layout.maxSize = Vec2{InfiniteLayoutSize, height};
    layout.clip = true;
    return node;
}

NodeHandle disabledIconButton(Context &ctx, NodeHandle parent, std::string_view name)
{
    const NodeHandle button = ctx.createButton(parent, "");
    fixed(ctx, button, 24.0f, 24.0f);

    auto layout = ctx.layoutStyle(button);
    layout.mode = LayoutMode::Horizontal;
    layout.horizontalAlignment = Alignment::Center;
    layout.verticalAlignment = Alignment::Center;
    layout.padding = Padding{};

    const NodeHandle icon = ctx.createIcon(button, name);
    ctx.setIconSize(icon, 16.0f);
    ctx.setIconColor(icon, ctx.theme().textMuted);
    ctx.setEnabled(button, false);
    return button;
}

NodeHandle flexibleLabel(Context &ctx, NodeHandle parent)
{
    const NodeHandle host = ctx.createContainer(parent);
    auto layout = ctx.layoutStyle(host);
    layout.mode = LayoutMode::Stack;
    layout.horizontalAlignment = Alignment::Stretch;
    layout.verticalAlignment = Alignment::Stretch;
    layout.clip = true;
    layout.minSize = Vec2{0.0f, 22.0f};
    layout.preferredSize = Vec2{0.0f, 22.0f};
    layout.maxSize = Vec2{InfiniteLayoutSize, 22.0f};

    const NodeHandle label = ctx.createLabel(host, "");
    ctx.style(label).all.text = ctx.theme().textMuted;
    return label;
}

std::string_view entryIcon(const ContentBrowserEntry &entry)
{
    if (entry.missing) return "warning";
    if (entry.folder) return "FileFolder";
    if (entry.type == Assets::AssetType::Model) return "box";
    if (entry.type == Assets::AssetType::Scene) return "file-code";
    return "file";
}
} // namespace

void ContentBrowserPanel::build(Context &ctx, NodeHandle parent)
{
    fillPane(ctx, parent);
    const NodeHandle panel = ctx.createPanel(parent);

    auto layout = ctx.layoutStyle(panel);
    layout.mode = LayoutMode::Vertical;
    layout.horizontalAlignment = Alignment::Stretch;
    layout.padding = Padding{};
    layout.gap = 0.0f;
    layout.clip = true;
    ctx.style(panel).shadow = ShadowStyle{};

    toolbar = bar(ctx, panel, 32.0f);
    const NodeHandle icon = ctx.createIcon(toolbar, "folder");
    ctx.setIconSize(icon, 16.0f);
    ctx.setIconColor(icon, ctx.theme().textMuted);

    title = ctx.createLabel(toolbar, "CONTENT BROWSER");
    const float titleWidth =
        std::ceil(ctx.font().measureText("CONTENT BROWSER").x) + 8.0f;
    fixed(ctx, title, titleWidth, 24.0f);
    ctx.style(title).all.text = ctx.theme().textMuted;

    importButton = ctx.createButton(toolbar, "Import");
    fixed(ctx, importButton, 60.0f, 26.0f);
    ctx.setEnabled(importButton, false);
    ctx.setTooltip(importButton, "Import one asset into the current folder.");

    searchInput = ctx.createTextInput(toolbar, "", "Search assets...");
    auto searchLayout = ctx.layoutStyle(searchInput);
    searchLayout.padding = Padding{8.0f, 2.0f};
    searchLayout.minSize = Vec2{0.0f, 26.0f};
    searchLayout.preferredSize = Vec2{0.0f, 26.0f};
    searchLayout.maxSize = Vec2{InfiniteLayoutSize, 26.0f};
    ctx.setEnabled(searchInput, false);

    typeFilter = ctx.createComboBox(toolbar, {"All types", "Models", "Audio", "Scenes"}, 0);
    fixed(ctx, typeFilter, 120.0f, 26.0f);
    ctx.layoutStyle(typeFilter).padding = Padding{8.0f, 2.0f};
    ctx.setEnabled(typeFilter, false);

    refreshButton = disabledIconButton(ctx, toolbar, "refresh");
    ctx.setTooltip(refreshButton, "Refresh project assets and folders.");

    const NodeHandle navigation = bar(ctx, panel, 28.0f);
    backButton = disabledIconButton(ctx, navigation, "chevron-left");
    forwardButton = disabledIconButton(ctx, navigation, "chevron-right");
    upButton = disabledIconButton(ctx, navigation, "chevron-up");
    ctx.setTooltip(backButton, "Previous folder");
    ctx.setTooltip(forwardButton, "Next folder");
    ctx.setTooltip(upButton, "Parent folder");

    breadcrumbs = ctx.createContainer(navigation);
    auto breadcrumbLayout = ctx.layoutStyle(breadcrumbs);
    breadcrumbLayout.mode = LayoutMode::Horizontal;
    breadcrumbLayout.verticalAlignment = Alignment::Center;
    breadcrumbLayout.clip = true;
    breadcrumbLayout.minSize = Vec2{0.0f, 24.0f};
    breadcrumbLayout.preferredSize = Vec2{0.0f, 24.0f};
    breadcrumbLayout.maxSize = Vec2{InfiniteLayoutSize, 24.0f};

    breadcrumbLabel = ctx.createLabel(breadcrumbs, "res://");
    ctx.style(breadcrumbLabel).all.text = ctx.theme().textMuted;

    errorLabel = ctx.createLabel(panel, "");
    auto errorLayout = ctx.layoutStyle(errorLabel);
    errorLayout.minSize = Vec2{0.0f, 24.0f};
    errorLayout.preferredSize = Vec2{0.0f, 24.0f};
    errorLayout.maxSize = Vec2{InfiniteLayoutSize, 24.0f};
    errorLayout.padding = Padding{8.0f, 0.0f};
    errorLayout.clip = true;
    ctx.setVisible(errorLabel, false);

    actionBar = ctx.createContainer(panel);
    auto actionLayout = ctx.layoutStyle(actionBar);
    actionLayout.mode = LayoutMode::Vertical;
    actionLayout.horizontalAlignment = Alignment::Stretch;
    actionLayout.padding = Padding{8.0f, 2.0f};
    actionLayout.gap = 2.0f;
    actionLayout.minSize = Vec2{0.0f, 56.0f};
    actionLayout.preferredSize = Vec2{0.0f, 56.0f};
    actionLayout.maxSize = Vec2{InfiniteLayoutSize, 56.0f};
    actionLayout.clip = true;

    moveRow = bar(ctx, actionBar, 26.0f);
    ctx.layoutStyle(moveRow).padding = Padding{};
    const NodeHandle moveLabel = ctx.createLabel(moveRow, "Move to");
    fixed(ctx, moveLabel, 72.0f, 26.0f);

    moveDestination = ctx.createComboBox(moveRow, {}, -1);
    auto moveLayout = ctx.layoutStyle(moveDestination);
    moveLayout.minSize = Vec2{0.0f, 26.0f};
    moveLayout.preferredSize = Vec2{0.0f, 26.0f};
    moveLayout.maxSize = Vec2{InfiniteLayoutSize, 26.0f};
    moveLayout.padding = Padding{8.0f, 2.0f};
    ctx.style(moveDestination).focusRing = FocusRingStyle{};
    ctx.style(moveDestination).focused.border = ctx.theme().accent;
    ctx.setVisible(moveRow, false);

    const NodeHandle actionButtons = bar(ctx, actionBar, 26.0f);
    ctx.layoutStyle(actionButtons).padding = Padding{};
    const NodeHandle warning = flexibleLabel(ctx, actionButtons);
    ctx.setText(warning, "Clears scene Undo/Redo.");

    actionCancel = ctx.createButton(actionButtons, "Cancel");
    fixed(ctx, actionCancel, 68.0f, 26.0f);

    actionConfirm = ctx.createButton(actionButtons, "Move");
    fixed(ctx, actionConfirm, 76.0f, 26.0f);

    for (NodeHandle button : {actionCancel, actionConfirm})
    {
        ctx.layoutStyle(button).padding = Padding{8.0f, 2.0f};
        ctx.style(button).focusRing = FocusRingStyle{};
        ctx.style(button).focused.border = ctx.theme().accent;
    }
    ctx.setVisible(actionBar, false);

    const NodeHandle split = ctx.createSplitContainer(panel, SplitOrientation::Horizontal, 0.22f);
    ctx.setSplitMinimumSizes(split, 160.0f, 240.0f);
    ctx.style(split).all.background = Color::transparent();
    ctx.style(split).shadow = ShadowStyle{};

    const NodeHandle foldersPane = ctx.splitFirstPane(split);
    const NodeHandle assetsPane = ctx.splitSecondPane(split);
    fillPane(ctx, foldersPane);
    fillPane(ctx, assetsPane);

    folderTree = ctx.createTreeView(foldersPane);
    auto treeLayout = ctx.layoutStyle(folderTree);
    treeLayout.minSize = Vec2{};
    treeLayout.preferredSize = Vec2{};
    treeLayout.clip = true;
    ctx.setTreeViewDragEnabled(folderTree, false);
    ctx.setTreeViewMaxIndentWidth(folderTree, 64.0f);
    ctx.style(folderTree).shadow = ShadowStyle{};

    assetScroll = ctx.createContainer(assetsPane);
    auto scrollLayout = ctx.layoutStyle(assetScroll);
    scrollLayout.mode = LayoutMode::Vertical;
    scrollLayout.horizontalAlignment = Alignment::Stretch;
    scrollLayout.padding = Padding{6.0f, 6.0f, 18.0f, 6.0f};
    scrollLayout.gap = 0.0f;
    scrollLayout.scrollY = true;
    scrollLayout.clip = true;

    grid = ctx.createContainer(assetScroll);
    auto gridLayout = ctx.layoutStyle(grid);
    gridLayout.mode = LayoutMode::Vertical;
    gridLayout.horizontalAlignment = Alignment::Stretch;
    gridLayout.gap = TileGap;
    gridLayout.padding = Padding{GridInset};
    gridLayout.fitContentY = true;

    emptyLabel = ctx.createLabel(assetScroll, "No project open.");
    ctx.style(emptyLabel).all.text = ctx.theme().textMuted;
    ctx.layoutStyle(emptyLabel).clip = true;

    const NodeHandle footer = bar(ctx, panel, 26.0f);
    selectionLabel = flexibleLabel(ctx, footer);
    countLabel = ctx.createLabel(footer, "0 items");
    fixed(ctx, countLabel, 108.0f, 22.0f);
    ctx.style(countLabel).all.text = ctx.theme().textMuted;
    ctx.setTextAlign(countLabel, TextAlign::End, TextAlign::Center);
}

void ContentBrowserPanel::clearTiles(Context &ctx)
{
    for (NodeHandle row : rows)
    {
        ctx.destroyNode(row);
    }
    rows.clear();
    tiles.clear();
    columns = 0;
    tilesDirty = true;
}

void ContentBrowserPanel::bindProject(Context &ctx, const Project::Project *project, const Assets::AssetRegistry &registry)
{
    cancelAction(ctx);
    deleteRequest = {};
    actionError.clear();

    bound = true;
    catalogPending = false;
    history.clear();
    historyCursor = 0;
    searchText.clear();
    appliedType = 0;
    ctx.setText(searchInput, "");
    ctx.setComboBoxSelectedIndex(typeFilter, 0);

    for (NodeHandle node : crumbNodes) ctx.destroyNode(node);
    crumbNodes.clear();
    crumbs.clear();
    crumbPaths.clear();
    crumbWidths.clear();
    crumbsDirty = true;

    if (!ctx.isValid(breadcrumbLabel))
    {
        breadcrumbLabel = ctx.createLabel(breadcrumbs, "");
        ctx.style(breadcrumbLabel).all.text = ctx.theme().textMuted;
    }
    boundProject = project ? project->getProjectFilePath() : std::filesystem::path{};

    clearTiles(ctx);
    ctx.clearTreeView(folderTree);
    index = ContentBrowserIndex{};
    folderItems.clear();
    visible.clear();
    currentFolder = "res://";
    selectedAsset = {};
    selectedFolder.clear();
    lastFolderClick.clear();
    ctx.setTooltip(selectionLabel, "");
    ctx.setTooltip(breadcrumbs, "");
    std::string error;

    if (project && index.rebuild(*project, registry, error))
    {
        rebuildTree(ctx, *project);
        showFolder(ctx, currentFolder);
    }
    else
    {
        ctx.setText(breadcrumbLabel, project ? "res://" : "No project");
        ctx.setText(selectionLabel, project ? "res://" : "No project open.");
        ctx.setText(countLabel, "0 items");
        ctx.setText(emptyLabel, project ? "Assets are unavailable." : "No project open.");
        ctx.setVisible(emptyLabel, true);
        ctx.setVisible(grid, false);
    }

    ctx.setText(errorLabel, error);
    ctx.setTooltip(errorLabel, error);
    ctx.setVisible(errorLabel, !error.empty());
}

void ContentBrowserPanel::rebuildTree(Context &ctx, const Project::Project &project)
{
    const auto entries = index.entries();
    folderItems.assign(entries.size(), TreeViewItemId{});

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const ContentBrowserEntry &entry = entries[i];
        if (!entry.folder) continue;

        TreeViewItemId parent{};
        if (const auto parentIndex = index.findFolder(entry.parent()))
        {
            parent = folderItems[*parentIndex];
        }

        std::string label = entry.path == "res://" ? project.getConfig().name : std::string(entry.name());
        if (label.empty()) label = "Project";
        if (entry.missing) label += " (missing)";

        folderItems[i] = ctx.addTreeViewItem(folderTree, parent, label, static_cast<u64>(i));
        ctx.setTreeViewItemExpanded(folderTree, folderItems[i], entry.path == "res://" || entry.parent() == "res://");
    }
}

void ContentBrowserPanel::select(Context &ctx, std::size_t entryIndex)
{
    const auto entries = index.entries();
    if (entryIndex >= entries.size()) return;

    const ContentBrowserEntry &entry = entries[entryIndex];
    selectedAsset = entry.folder ? Assets::AssetId{} : entry.assetId;
    selectedFolder = entry.folder ? entry.path : std::string{};
    ctx.setText(selectionLabel, entry.path);
    ctx.setTooltip(selectionLabel, entry.path);
    styleSelection(ctx);
}

void ContentBrowserPanel::styleSelection(Context &ctx)
{
    const auto entries = index.entries();
    const Theme &theme = ctx.theme();

    for (const Tile &tile : tiles)
    {
        if (tile.entry >= entries.size()) continue;

        const ContentBrowserEntry &entry = entries[tile.entry];
        const bool selected = entry.folder ? selectedFolder == entry.path : selectedAsset == entry.assetId;

        auto style = ctx.style(tile.button);
        style.normal.background = selected ? theme.surfaceActive : Color::transparent();
        style.normal.border = selected ? theme.accent : Color::transparent();
        style.focused.background = selected ? theme.surfaceActive : Color::transparent();
        style.focused.border = selected ? theme.accent : theme.borderStrong;
    }
}

void ContentBrowserPanel::rebuildTiles(Context &ctx)
{
    const bool filtered = appliedType != 0 ||
        searchText.find_first_not_of(" \t\r\n") != std::string::npos;
    const float tileHeight = filtered ? 112.0f : TileHeight;
    const float width =
        ctx.tree().rect(assetScroll).w - 24.0f - GridInset * 2.0f;
    if (!std::isfinite(width) || width <= 0.0f) return;

    const auto wanted = static_cast<std::size_t>(std::clamp(
        std::floor((width + TileGap) / (TileWidth + TileGap)),
        1.0f, 256.0f));
    if (!tilesDirty && columns == wanted) return;

    // Keep a pressed tile intact until its release has been processed
    for (const Tile &tile : tiles)
    {
        if (ctx.isActive(tile.button)) return;
    }

    clearTiles(ctx);
    columns = wanted;
    const auto entries = index.entries();
    tiles.reserve(visible.size());
    NodeHandle row{};

    for (std::size_t i = 0; i < visible.size(); ++i)
    {
        const std::size_t entryIndex = visible[i];
        if (entryIndex >= entries.size()) break;

        const ContentBrowserEntry &entry = entries[entryIndex];

        if (i % columns == 0)
        {
            row = ctx.createContainer(grid);
            rows.push_back(row);

            auto layout = ctx.layoutStyle(row);
            layout.mode = LayoutMode::Horizontal;
            layout.verticalAlignment = Alignment::Center;
            layout.gap = TileGap;
            layout.minSize = Vec2{0.0f, tileHeight};
            layout.preferredSize = Vec2{0.0f, tileHeight};
            layout.maxSize = Vec2{InfiniteLayoutSize, tileHeight};
        }

        const NodeHandle button = ctx.createButton(row, "");
        fixed(ctx, button, TileWidth, tileHeight);

        auto layout = ctx.layoutStyle(button);
        layout.mode = LayoutMode::Vertical;
        layout.horizontalAlignment = Alignment::Center;
        layout.verticalAlignment = Alignment::Center;
        layout.padding = Padding{6.0f};
        layout.gap = 4.0f;
        layout.clip = true;

        auto style = ctx.style(button);
        style.cornerRadius = ctx.theme().smallRadius;
        style.shadow = ShadowStyle{};
        style.hovered.background = ctx.theme().surfaceHovered;
        style.active.background = ctx.theme().surfaceActive;

        const NodeHandle icon = ctx.createIcon(button, entryIcon(entry));
        ctx.setIconSize(icon, 44.0f);
        if (!entry.folder || entry.missing)
        {
            ctx.setIconColor(icon, ctx.theme().textMuted);
        }

        const NodeHandle label = ctx.createLabel(button, caption(ctx, entry.name(), TileWidth - 12.0f));
        fixed(ctx, label, TileWidth - 12.0f, 22.0f);
        ctx.layoutStyle(label).clip = true;
        ctx.setTextAlign(label, TextAlign::Center, TextAlign::Center);
        ctx.style(label).all.text = entry.missing ? ctx.theme().textMuted : ctx.theme().text;

        if (filtered)
        {
            std::string_view relative = entry.parent();
            relative.remove_prefix(currentFolder.size());
            if (relative.empty()) relative = ".";

            const NodeHandle location = ctx.createLabel(
                button, caption(ctx, relative, TileWidth - 12.0f));
            fixed(ctx, location, TileWidth - 12.0f, 22.0f);
            ctx.layoutStyle(location).clip = true;
            ctx.setTextAlign(location, TextAlign::Center, TextAlign::Center);
            ctx.style(location).all.text = ctx.theme().textMuted;
        }

        std::string tooltip = entry.missing ? "Missing | " : "";
        tooltip += entry.folder ? "Folder" : std::string(Assets::assetTypeName(entry.type));
        tooltip += " | " + entry.path;
        ctx.setTooltip(button, tooltip);

        tiles.push_back(Tile{button, entryIndex});
    }

    tilesDirty = false;
    styleSelection(ctx);
}

} // namespace Cthulhu::Editor