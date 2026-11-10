#include "contentBrowserPanel.hpp"

#include <algorithm>
#include <cassert>
#include <span>

#include "assetRegistry.hpp"
#include "session.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
namespace
{
constexpr u64 OpenLocation = 1;
constexpr u64 DeleteAsset = 2;
}

void ContentBrowserPanel::cancelDelete(Context &ctx)
{
    if (ctx.isFocused(deleteConfirm) || ctx.isFocused(deleteCancel))
        ctx.clearFocus();

    pendingDelete = {};
    deletePath.clear();
    ctx.setVisible(deleteBar, false);
}

bool ContentBrowserPanel::updateActions(Context &ctx, const Assets::AssetRegistry &registry, Scribe::Session &session, bool canEdit)
{
    assert(pendingDelete.isValid() == !deletePath.empty());

    const auto showError = [&](std::string_view message) {
        actionError.assign(message);
        ctx.setText(errorLabel, actionError);
        ctx.setTooltip(errorLabel, actionError);
        ctx.setVisible(errorLabel, !actionError.empty());
    };

    if (pendingDelete.isValid() && (!canEdit || catalogPending))
        cancelDelete(ctx);

    ContextMenuSelection choice;
    if (ctx.pollContextMenuSelection(choice))
    {
        const auto tile = std::find_if(tiles.begin(), tiles.end(), [&](const Tile &value) { return value.button == choice.owner; });
        const auto entries = index.entries();
        if (catalogPending || tile == tiles.end() || tile->entry >= entries.size())
            return true;

        const auto &entry = entries[tile->entry];
        if (choice.action == OpenLocation)
        {
            showFolder(ctx, entry.folder ? std::string_view(entry.path) : entry.parent());
            if (!entry.folder)
            {
                for (std::size_t visibleEntry : visible)
                {
                    if (entries[visibleEntry].assetId == entry.assetId)
                    {
                        select(ctx, visibleEntry);
                        break;
                    }
                }
            }
        }
        else if (choice.action == DeleteAsset && canEdit && !entry.folder)
        {
            const auto *record = registry.findById(entry.assetId);
            if (!record || record->path != entry.path)
            {
                showError("Asset changed. Refresh and select it again.");
                return true;
            }

            deletePath = record->path;
            pendingDelete = record->id;
            const std::string question = std::string(record->missing ? "Forget missing asset: " : "Recycle asset: ") + deletePath;

            ctx.setText(deleteLabel, question);
            ctx.setTooltip(deleteLabel, question);
            ctx.setText(deleteConfirm, record->missing ? "Forget" : "Recycle");
            ctx.setVisible(deleteBar, true);
            ctx.setFocused(deleteCancel);
            showError("");
        }
        return true;
    }

    if (ctx.contextMenuOwnsInput()) return true;

    if (pendingDelete.isValid())
    {
        const auto *record = registry.findById(pendingDelete);
        if (!record || record->path != deletePath)
        {
            cancelDelete(ctx);
            showError("Asset changed. Refresh and select it again.");
            return true;
        }

        bool cancel = ctx.isClicked(deleteCancel);
        bool confirm = ctx.isClicked(deleteConfirm);
        for (const InputEvent &event : ctx.input().events)
        {
            if (event.type != InputEventType::Key || event.action != KeyAction::Press)
                continue;

            const bool onCancel = ctx.isFocused(deleteCancel);
            const bool onConfirm = ctx.isFocused(deleteConfirm);
            if (event.key == Key::Escape) cancel = true;
            else if (event.key == Key::Tab && (onCancel || onConfirm))
                ctx.setFocused(onCancel ? deleteConfirm : deleteCancel);
            else if (event.key == Key::Enter || event.key == Key::Space)
            {
                cancel = cancel || onCancel;
                confirm = confirm || onConfirm;
            }
        }

        if (cancel) cancelDelete(ctx);
        else if (confirm)
        {
            const Assets::AssetId id = pendingDelete;
            cancelDelete(ctx);
            const auto result = session.deleteAsset(id);
            showError(result.error);
        }
        return true;
    }

    if (catalogPending) return false;

    const bool keyboardMenu = std::any_of(
        ctx.input().events.begin(), ctx.input().events.end(),
        [](const InputEvent &event) {
            return event.type == InputEventType::Key && event.action == KeyAction::Press &&
                   (event.key == Key::Menu ||
                    (event.key == Key::F10 && event.modifiers.shift));
        });
    const bool rightClick = ctx.input().mouse.pressed(MouseButton::Right);
    if (!keyboardMenu && !rightClick) return false;

    const auto entries = index.entries();
    for (const Tile &tile : tiles)
    {
        if (!(rightClick && ctx.isHovered(tile.button)) && !(keyboardMenu && ctx.isFocused(tile.button)))
            continue;
        if (tile.entry >= entries.size()) break;

        const auto &entry = entries[tile.entry];
        const ContextMenuItem items[] = {
            {OpenLocation, entry.folder ? "Open" : "Show in folder", true},
            {DeleteAsset, entry.missing ? "Forget missing asset..." : "Delete...", canEdit}
        };

        Vec2 position = ctx.input().mouse.position;
        if (keyboardMenu)
        {
            const Rect rect = ctx.tree().rect(tile.button);
            position = Vec2{rect.x, rect.maxY() - ctx.scrollOffsetY(assetScroll)};
        }

        if (ctx.openContextMenu(tile.button, position, std::span<const ContextMenuItem>{items}.first(entry.folder ? 1u : 2u)))
        {
            select(ctx, tile.entry);
            lastFolderClick.clear();
            return true;
        }
        break;
    }
    return false;
}
} // namespace Cthulhu::Editor