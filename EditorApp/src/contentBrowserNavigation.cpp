#include "contentBrowserPanel.hpp"

#include <algorithm>
#include <cmath>

#include "project.hpp"
#include "engine.hpp"
#include "session.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
namespace
{
std::string_view breadcrumbName(std::string_view path)
{
    if (path == "res://") return path;
    if (!path.empty() && path.back() == '/') path.remove_suffix(1);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}
} // namespace

std::string ContentBrowserPanel::caption(Context &ctx, std::string_view text, float width)
{
    Font &font = ctx.font();
    if (font.measureText(text).x <= width) return std::string(text);

    constexpr std::string_view suffix = "...";
    const float available = std::max(0.0f, width - font.measureText(suffix).x);
    std::size_t end = std::min<std::size_t>(font.textOffsetAtX(text, available), text.size());

    while (end > 0 && font.measureText(text.substr(0, end)).x > available)
    {
        --end;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0u) == 0x80u)
        {
            --end;
        }
    }

    return std::string(text.substr(0, end)) + std::string(suffix);
}

void ContentBrowserPanel::applyFilter(Context &ctx)
{
    std::optional<Assets::AssetType> type;
    switch (appliedType)
    {
        case 1: type = Assets::AssetType::Model; break;
        case 2: type = Assets::AssetType::Audio; break;
        case 3: type = Assets::AssetType::Scene; break;
        default: break;
    }

    (void)index.query(currentFolder, searchText, type, visible);
    tilesDirty = true;
    lastFolderClick.clear();
    ctx.setScrollOffsetY(assetScroll, 0.0f);

    const auto entries = index.entries();
    std::optional<std::size_t> selected;
    for (std::size_t entry : visible)
    {
        const auto &value = entries[entry];
        if ((value.folder && !selectedFolder.empty() &&
             value.path == selectedFolder) ||
            (!value.folder && selectedAsset.isValid() &&
             value.assetId == selectedAsset))
        {
            selected = entry;
            break;
        }
    }

    if (selected)
    {
        select(ctx, *selected);
    }
    else
    {
        selectedAsset = {};
        selectedFolder.clear();
        ctx.setText(selectionLabel, currentFolder);
        ctx.setTooltip(selectionLabel, currentFolder);
        styleSelection(ctx);
    }

    const bool filtered = type.has_value() || searchText.find_first_not_of(" \t\r\n") != std::string::npos;
    ctx.setText(emptyLabel, filtered ? "No matching assets." : "This folder is empty.");
    ctx.setVisible(emptyLabel, visible.empty());
    ctx.setVisible(grid, !visible.empty());
    ctx.setText(countLabel, std::to_string(visible.size()) + (visible.size() == 1 ? " item" : " items"));
}

void ContentBrowserPanel::showFolder(Context &ctx, std::string_view path, bool recordHistory)
{
    const auto folder = index.findFolder(path);
    if (!folder) return;

    const std::string destination(path);
    if (recordHistory && (history.empty() || history[historyCursor] != destination))
    {
        if (!history.empty()) history.resize(historyCursor + 1);
        history.push_back(destination);
        if (history.size() > 128) history.erase(history.begin());
        historyCursor = history.size() - 1;
    }

    currentFolder = destination;
    selectedAsset = {};
    selectedFolder.clear();
    crumbPaths.clear();
    crumbsDirty = true;

    const auto entries = index.entries();
    for (auto ancestor = folder; ancestor; ancestor = index.findFolder(entries[*ancestor].parent()))
    {
        ctx.setTreeViewItemExpanded(folderTree, folderItems[*ancestor], true);
    }

    ctx.setTreeViewSelectedItem(folderTree, folderItems[*folder]);
    ctx.setTooltip(breadcrumbs, currentFolder);
    applyFilter(ctx);
}

void ContentBrowserPanel::rebuildBreadcrumbs(Context &ctx)
{
    const auto current = index.findFolder(currentFolder);
    if (!current) return;

    for (const Crumb &crumb : crumbs)
    {
        if (ctx.isActive(crumb.button)) return;
    }

    const auto entries = index.entries();
    if (crumbPaths.empty())
    {
        for (auto folder = current; folder;
             folder = index.findFolder(entries[*folder].parent()))
        {
            crumbPaths.push_back(entries[*folder].path);
        }

        std::reverse(crumbPaths.begin(), crumbPaths.end());
        crumbWidths.clear();
        for (const std::string &path : crumbPaths)
        {
            const std::string_view name = breadcrumbName(path);
            crumbWidths.push_back(std::clamp(std::ceil(ctx.font().measureText(name).x) + 12.0f, 36.0f, 144.0f));
        }
    }

    const float available = ctx.tree().rect(breadcrumbs).w;
    std::size_t first = crumbPaths.size() > 1 ? crumbPaths.size() - 1 : 1;
    float used = crumbWidths.front();

    if (crumbPaths.size() > 1)
    {
        used += 16.0f + crumbWidths.back();
        while (first > 1)
        {
            const float extra = 16.0f + crumbWidths[first - 1];
            const float omitted = first > 2 ? 40.0f : 0.0f;
            if (used + extra + omitted > available) break;
            used += extra;
            --first;
        }
    }

    if (!crumbsDirty && first == firstCrumb) return;
    firstCrumb = first;
    crumbsDirty = false;

    if (ctx.isValid(breadcrumbLabel))
    {
        ctx.destroyNode(breadcrumbLabel);
        breadcrumbLabel = {};
    }

    for (NodeHandle node : crumbNodes) ctx.destroyNode(node);
    crumbNodes.clear();
    crumbs.clear();

    const auto separator = [&](std::string_view text, float width) {
        const NodeHandle node = ctx.createLabel(breadcrumbs, text);
        auto layout = ctx.layoutStyle(node);
        layout.minSize = Vec2{width, 24.0f};
        layout.preferredSize = Vec2{width, 24.0f};
        layout.maxSize = Vec2{width, 24.0f};
        ctx.setTextAlign(node, TextAlign::Center, TextAlign::Center);
        ctx.style(node).all.text = ctx.theme().textMuted;
        crumbNodes.push_back(node);
    };

    const auto button = [&](std::size_t i) {
        const std::string_view name = breadcrumbName(crumbPaths[i]);
        const NodeHandle node = ctx.createButton(breadcrumbs, caption(ctx, name, crumbWidths[i] - 12.0f));
        auto layout = ctx.layoutStyle(node);
        layout.minSize = Vec2{crumbWidths[i], 24.0f};
        layout.preferredSize = Vec2{crumbWidths[i], 24.0f};
        layout.maxSize = Vec2{crumbWidths[i], 24.0f};
        layout.padding = Padding{6.0f, 2.0f};
        layout.clip = true;

        auto style = ctx.style(node);
        style.normal.background = Color::transparent();
        style.normal.border = Color::transparent();
        style.shadow = ShadowStyle{};
        style.focusRing = FocusRingStyle{};

        ctx.setEnabled(node, i + 1 < crumbPaths.size());
        ctx.setTooltip(node, crumbPaths[i]);
        crumbNodes.push_back(node);
        crumbs.push_back(Crumb{node, crumbPaths[i]});
    };

    button(0);
    if (first > 1)
    {
        separator(">", 16.0f);
        separator("...", 24.0f);
    }

    for (std::size_t i = first; i < crumbPaths.size(); ++i)
    {
        separator(">", 16.0f);
        button(i);
    }
}

void ContentBrowserPanel::update(Context &ctx, Engine &engine, Scribe::Session &session, bool assetsChanged)
{
    const auto *project = engine.getProject();
    const auto &registry = engine.getAssetManager().getRegistry();
    const bool canEditAssets = project && !engine.isPlaying() && !session.transformEditActive();

    catalogPending = catalogPending || assetsChanged;

    const bool changed = !bound || (project ? boundProject != project->getProjectFilePath() : !boundProject.empty());
    const bool actionHandled = !changed && updateActions(ctx, registry, session, canEditAssets);

    if (changed)
    {
        bindProject(ctx, project, registry);
    }
    else if (catalogPending)
    {
        bool busy = ctx.contextMenuOwnsInput() || ctx.isActive(folderTree);
        for (const Tile &tile : tiles)
            busy = busy || ctx.isActive(tile.button);
        for (const Crumb &crumb : crumbs)
            busy = busy || ctx.isActive(crumb.button);

        if (!busy)
        {
            catalogPending = false;
            if (project) reloadCatalog(ctx, *project, registry);
        }
    }
    else if (!actionHandled && !ctx.contextMenuOwnsInput())
    {
        if (canEditAssets && ctx.isClicked(refreshButton))
        {
            const auto result = session.refreshAssets();
            actionError = result.error;
            ctx.setText(errorLabel, result.error);
            ctx.setTooltip(errorLabel, result.error);
            ctx.setVisible(errorLabel, !result.ok());
        }

        if (std::isfinite(ctx.input().deltaTime) && ctx.input().deltaTime > 0.0f)
        {
            folderClickAge = std::min(1.0f, folderClickAge + ctx.input().deltaTime);
        }

        if (ctx.isClicked(backButton) && historyCursor > 0)
        {
            --historyCursor;
            showFolder(ctx, history[historyCursor], false);
        }
        else if (ctx.isClicked(forwardButton) && historyCursor + 1 < history.size())
        {
            ++historyCursor;
            showFolder(ctx, history[historyCursor], false);
        }
        else if (ctx.isClicked(upButton))
        {
            if (const auto folder = index.findFolder(currentFolder))
            {
                showFolder(ctx, index.entries()[*folder].parent());
            }
        }
        else
        {
            for (const Crumb &crumb : crumbs)
            {
                if (ctx.isClicked(crumb.button))
                {
                    const std::string destination = crumb.path;
                    showFolder(ctx, destination);
                    break;
                }
            }
        }

        const auto item = ctx.treeViewSelectedItem(folderTree);
        if (ctx.treeViewItemExists(folderTree, item))
        {
            const u64 value = ctx.treeViewItemUserValue(folderTree, item);
            const auto entries = index.entries();
            if (value < entries.size() && entries[static_cast<std::size_t>(value)].folder && entries[static_cast<std::size_t>(value)].path != currentFolder)
            {
                showFolder(ctx, entries[static_cast<std::size_t>(value)].path);
            }
        }

        bool tilePressed = false;
        for (const Tile &tile : tiles)
        {
            tilePressed = tilePressed || ctx.isActive(tile.button);
            if (!ctx.isClicked(tile.button)) continue;

            const auto entries = index.entries();
            if (tile.entry >= entries.size()) break;
            const auto &entry = entries[tile.entry];

            if (entry.folder && lastFolderClick == entry.path && folderClickAge <= 0.35f)
            {
                showFolder(ctx, entry.path);
            }
            else
            {
                select(ctx, tile.entry);
                lastFolderClick = entry.folder ? entry.path : std::string{};
                folderClickAge = 0.0f;
            }
            break;
        }

        const int nextType = std::clamp(ctx.comboBoxSelectedIndex(typeFilter), 0, 3);
        if (!tilePressed && index.findFolder(currentFolder) && (searchText != ctx.text(searchInput) || appliedType != nextType))
        {
            searchText.assign(ctx.text(searchInput));
            appliedType = nextType;
            applyFilter(ctx);
        }
    }

    const bool ready = project && index.findFolder(currentFolder).has_value() && !pendingDelete.isValid();
    ctx.setEnabled(folderTree, ready);
    ctx.setEnabled(assetScroll, ready);
    ctx.setEnabled(breadcrumbs, ready);
    ctx.setEnabled(searchInput, ready);
    ctx.setEnabled(typeFilter, ready);
    ctx.setEnabled(refreshButton, canEditAssets && !catalogPending && !pendingDelete.isValid());
    ctx.setEnabled(backButton, ready && historyCursor > 0);
    ctx.setEnabled(forwardButton, ready && historyCursor + 1 < history.size());
    ctx.setEnabled(upButton, ready && currentFolder != "res://");
    ctx.setVisible(title, ctx.tree().rect(toolbar).w >= 640.0f);

    if (!ctx.contextMenuOwnsInput())
    {
        rebuildBreadcrumbs(ctx);
        rebuildTiles(ctx);
    }
}
} // namespace Cthulhu::Editor