#include <algorithm>
#include <cassert>
#include <span>
#include <limits>

#include "contentBrowserPanel.hpp"

#include "assetRegistry.hpp"
#include "session.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
namespace
{
constexpr u64 OpenLocation = 1;
constexpr u64 DeleteAsset = 2;
constexpr u64 MoveAsset = 3;
constexpr u64 OpenScene = 4;
}

void ContentBrowserPanel::cancelAction(Context &ctx)
{
    if (ctx.isFocused(actionConfirm) || ctx.isFocused(actionCancel) || ctx.isFocused(moveDestination)) ctx.clearFocus();

    pendingAsset = {};
    sourcePath.clear();
    ctx.setVisible(actionBar, false);
    ctx.setVisible(moveRow, false);
    ctx.setVisible(actionLabel, true);
    pendingAction = PendingAction::Delete;

    auto layout = ctx.layoutStyle(actionBar);
    layout.minSize = Vec2{0.0f, 56.0f};
    layout.preferredSize = Vec2{0.0f, 56.0f};
    layout.maxSize = Vec2{InfiniteLayoutSize, 56.0f};
}

bool ContentBrowserPanel::updateActions(Context &ctx,
                                       const Project::Project *project,
                                       const Assets::AssetRegistry &registry,
                                       Scribe::Session &session,
                                       bool canEdit)
{
    assert(pendingAsset.isValid() == !sourcePath.empty());

    const auto showError = [&](std::string_view message) {
        actionError.assign(message);
        ctx.setText(errorLabel, actionError);
        ctx.setTooltip(errorLabel, actionError);
        ctx.setVisible(errorLabel, !actionError.empty());
    };

    if (pendingAsset.isValid() && (!canEdit || catalogPending))
        cancelAction(ctx);

    ContextMenuSelection choice;
    if (ctx.pollContextMenuSelection(choice))
    {
        const auto tile = std::find_if(tiles.begin(), tiles.end(), [&](const Tile &value) { return value.button == choice.owner; });
        const auto entries = index.entries();
        if (catalogPending || tile == tiles.end() || tile->entry >= entries.size())
            return true;

        const auto &entry = entries[tile->entry];
        if (choice.action == OpenScene)
        {
            if (canEdit && entry.type == Assets::AssetType::Scene && !entry.missing)
                sceneOpenRequest = entry.assetId;
        }
        else if (choice.action == OpenLocation)
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
        else if ((choice.action == DeleteAsset || choice.action == MoveAsset) && canEdit && !entry.folder)
        {
            const auto *record = registry.findById(entry.assetId);
            if (!record || record->path != entry.path)
            {
                showError("Asset changed. Refresh and select it again.");
                return true;
            }

            if (choice.action == MoveAsset)
            {
                if (record->missing)
                    showError("A missing asset cannot be moved.");
                else if (beginMove(ctx, entry))
                    showError("");
                else
                    showError("No destination folders are available.");
                return true;
            }

            pendingAction = PendingAction::Delete;

            sourcePath = record->path;
            pendingAsset = record->id;
            const std::string question = std::string(record->missing ? "Forget missing asset: " : "Recycle asset: ") + sourcePath;

            ctx.setText(actionLabel, question);
            ctx.setTooltip(actionLabel, question);
            ctx.setText(actionConfirm, record->missing ? "Forget" : "Recycle");
            ctx.setTooltip(actionConfirm, "Other saved scenes may still reference this asset.");
            ctx.setVisible(actionBar, true);
            ctx.setFocused(actionCancel);
            showError("");
        }
        return true;
    }

    if (ctx.contextMenuOwnsInput()) return true;

    if (pendingAsset.isValid())
    {
        const auto *record = registry.findById(pendingAsset);
        if (!record || record->path != sourcePath)
        {
            cancelAction(ctx);
            showError("Asset changed. Refresh and select it again.");
            return true;
        }

        const bool moving = pendingAction == PendingAction::Move;
        if (moving && record->missing)
        {
            cancelAction(ctx);
            showError("The source asset is missing. Refresh and select it again.");
            return true;
        }

        bool cancel = ctx.isClicked(actionCancel);
        bool confirm = ctx.isClicked(actionConfirm);
        for (const InputEvent &event : ctx.input().events)
        {
            if (event.type != InputEventType::Key ||
                event.action != KeyAction::Press)
                continue;

            const bool onCancel = ctx.isFocused(actionCancel);
            const bool onConfirm = ctx.isFocused(actionConfirm);
            const bool onFolder = moving && ctx.isFocused(moveDestination);

            if (event.key == Key::Escape) cancel = true;
            else if (event.key == Key::Tab)
            {
                const NodeHandle controls[] = {
                    moveDestination, actionCancel, actionConfirm};
                const std::size_t first = moving ? 0u : 1u;

                for (std::size_t i = first; i < 3; ++i)
                {
                    if (!ctx.isFocused(controls[i])) continue;
                    const std::size_t next = event.modifiers.shift
                        ? (i == first ? 2 : i - 1)
                        : (i == 2 ? first : i + 1);
                    ctx.setFocused(controls[next]);
                    break;
                }
            }
            else if (onFolder &&
                     (event.key == Key::Up || event.key == Key::Down ||
                      event.key == Key::Home || event.key == Key::End))
            {
                i32 selected = ctx.comboBoxSelectedIndex(moveDestination);
                if (event.key == Key::Home) selected = 0;
                else if (event.key == Key::End)
                    selected = std::numeric_limits<i32>::max();
                else if (event.key == Key::Up && selected > 0) --selected;
                else if (event.key == Key::Down &&
                         selected < std::numeric_limits<i32>::max())
                    ++selected;
                ctx.setComboBoxSelectedIndex(moveDestination, selected);
            }
            else if (event.key == Key::Enter || event.key == Key::Space)
            {
                if (onFolder) ctx.setFocused(actionCancel);
                cancel = cancel || onCancel;
                confirm = confirm || onConfirm;
            }
        }

        if (moving)
            ctx.setTooltip(
                moveDestination, ctx.comboBoxSelectedText(moveDestination));

        if (cancel) cancelAction(ctx);
        else if (confirm)
        {
            Scribe::Result result;
            if (moving)
                result = project ? moveAssetToFolder(ctx, *project, registry, session, *record,
                                                     ctx.comboBoxSelectedText(moveDestination))
                                 : Scribe::Result::failed("No project is open.");
            else
                result = session.deleteAsset(pendingAsset);

            cancelAction(ctx);
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
        const bool scene = !entry.folder && entry.type == Assets::AssetType::Scene;

        const ContextMenuItem items[] = {
            {OpenScene, "Open Scene", canEdit && !entry.missing},
            {OpenLocation, entry.folder ? "Open" : "Show in folder", true},
            {MoveAsset, "Move to...", canEdit && !entry.missing},
            {DeleteAsset, entry.missing ? "Forget missing asset..." : "Delete...", canEdit}
        };

        Vec2 position = ctx.input().mouse.position;
        if (keyboardMenu)
        {
            const Rect rect = ctx.tree().rect(tile.button);
            position = Vec2{rect.x, rect.maxY() - ctx.scrollOffsetY(assetScroll)};
        }

        auto menu = std::span<const ContextMenuItem>{items};
        if (!scene)
            menu = menu.subspan(1);
        if (entry.folder)
            menu = menu.first(1);

        if (ctx.openContextMenu(tile.button, position, menu))
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