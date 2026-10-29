#include "hierarchyPanel.hpp"

#include "engine.hpp"
#include "components.hpp"
#include "scene.hpp"
#include "log_utils.hpp"
#include "session.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

using namespace octogui;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
    namespace
    {
        struct IconButton
        {
            NodeHandle button{};
            NodeHandle icon{};
        };

        constexpr f32 HierarchyRowHeight = 26.0f;
        constexpr f32 HierarchyIndentWidth = 16.0f;
        constexpr f32 HierarchyChevronWidth = 16.0f;
        constexpr f32 HierarchyTextGap = 4.0f;
        constexpr f32 HierarchyIndentCap = 85.0f;

        std::string trimEntityName(std::string_view value)
        {
            std::size_t first{};
            std::size_t last = value.size();

            while (first < last && std::isspace(static_cast<unsigned char>(value[first])))
            {
                ++first;
            }
            while (first < last && std::isspace(static_cast<unsigned char>(value[last - 1])))
            {
                --last;
            }

            return std::string(value.substr(first, last-first));
        }

        std::string uniqueEntityName(const Scene::Scene &scene, std::string_view baseName, std::optional<Scene::EntityId> ignoredEntity = std::nullopt)
        {
            const auto isInUse = [&](std::string_view name)
            {
                const auto found = scene.findEntityByName(name);
                return found && (!ignoredEntity || *found != *ignoredEntity);
            };

            std::string candidate(baseName);
            if (!isInUse(candidate))
            {
                return candidate;
            }

            for (std::size_t suffix = 2;; ++suffix)
            {
                candidate = std::string(baseName) + std::to_string(suffix);
                if (!isInUse(candidate))
                {
                    return candidate;
                }
            }
        }

        bool hierarchyItemTextRect(Context &ctx, NodeHandle treeView, TreeViewItemId item, Rect &outRect)
        {
            const TreeViewState *state = ctx.tree().treeViewState(treeView);
            if (!state)
            {
                return false;
            }

            const auto visible = std::find_if(state->visibleItems.begin(), state->visibleItems.end(), [item](const TreeViewVisibleItem &entry) { return entry.item == item; });

            if (visible == state->visibleItems.end())
            {
                return false;
            }

            const LayoutStyle *layout = ctx.tree().layoutStyle(treeView);
            const Padding padding = layout ? layout->padding : Padding{};
            const Rect outer = ctx.tree().rect(treeView);
            const f32 contentX = outer.x + padding.left;
            const f32 contentY = outer.y + padding.top;
            const f32 contentWidth = std::max(0.0f, outer.w - padding.horizontal());

            const f32 indent = std::min(static_cast<f32>(visible->depth) * HierarchyIndentWidth, HierarchyIndentCap);
            const f32 textX = contentX + indent + HierarchyChevronWidth + HierarchyTextGap;
            const f32 textWidth = contentX + contentWidth - textX - 4.0f;

            if (textWidth <= 0.0f)
            {
                return false;
            }

            const f32 rowIndex = static_cast<f32>(visible - state->visibleItems.begin());

            outRect = Rect{textX, contentY + rowIndex * HierarchyRowHeight - state->scrollY, textWidth, HierarchyRowHeight};

            return true;
        }

        void fillWithChild(Context &ctx, NodeHandle pane)
        {
            auto layout = ctx.layoutStyle(pane);
            layout.mode = LayoutMode::Stack;
            layout.horizontalAlignment = Alignment::Stretch;
            layout.verticalAlignment = Alignment::Stretch;
        }

        NodeHandle panelHeader(Context &ctx, NodeHandle panel, std::string_view icon, std::string_view title, IconButton *outLeadingAction = nullptr)
        {
            const NodeHandle header = ctx.createContainer(panel);

            auto layout = ctx.layoutStyle(header);
            layout.mode = LayoutMode::Horizontal;
            layout.gap = 6.0f;
            layout.verticalAlignment = Alignment::Center;
            layout.minSize = Vec2{0.0f, 20.0f};
            layout.preferredSize = Vec2{0.0f, 20.0f};
            layout.maxSize = Vec2{InfiniteLayoutSize, 20.0f};

            if (outLeadingAction)
            {
                layout.minSize = Vec2{0.0f, 26.0f};
                layout.preferredSize = Vec2{0.0f, 26.0f};
                layout.maxSize = Vec2{InfiniteLayoutSize, 26.0f};

                outLeadingAction->button = ctx.createButton(header, "");
                auto actionLayout = ctx.layoutStyle(outLeadingAction->button);
                actionLayout.mode = LayoutMode::Horizontal;
                actionLayout.horizontalAlignment = Alignment::Center;
                actionLayout.verticalAlignment = Alignment::Center;
                actionLayout.padding = Padding{};
                actionLayout.minSize = Vec2{24.0f, 24.0f};
                actionLayout.preferredSize = Vec2{24.0f, 24.0f};
                actionLayout.maxSize = Vec2{24.0f, 24.0f};

                outLeadingAction->icon = ctx.createIcon(outLeadingAction->button, "plus");
                ctx.setIconSize(outLeadingAction->icon, 16.0f);
                ctx.setIconColor(outLeadingAction->icon, ctx.theme().textMuted);
            }

            const NodeHandle headerIcon = ctx.createIcon(header, icon);
            ctx.setIconSize(headerIcon, 16.0f);
            ctx.setIconColor(headerIcon, ctx.theme().textMuted);

            const NodeHandle heading = ctx.createLabel(header, title);
            ctx.style(heading).all.text = ctx.theme().textMuted;

            return header;
        }

        NodeHandle titledPanel(Context &ctx, NodeHandle pane, std::string_view icon, std::string_view title, NodeHandle *outHeader = nullptr, IconButton *outLeadingAction = nullptr)
        {
            fillWithChild(ctx, pane);

            const NodeHandle panel = ctx.createPanel(pane);

            auto layout = ctx.layoutStyle(panel);
            layout.mode = LayoutMode::Vertical;
            layout.horizontalAlignment = Alignment::Stretch;
            layout.verticalAlignment = Alignment::Stretch;
            layout.gap = 6.0f;
            layout.padding = Padding{8.0f};

            const NodeHandle header = panelHeader(ctx, panel, icon, title, outLeadingAction);
            if (outHeader)
            {
                *outHeader = header;
            }

            return panel;
        }
    }

    void HierarchyPanel::build(Context &ctx, NodeHandle parent)
    {
        IconButton action{};
        hierarchy = titledPanel(ctx, parent, "sidebar-left", "HIERARCHY", &hierarchyHeader, &action);
        createEntityButton = action.button;
        createEntityIcon = action.icon;
        hierarchyTree = ctx.createTreeView(hierarchy);
        ctx.setTreeViewMaxIndentWidth(hierarchyTree, HierarchyIndentCap);
        auto layout = ctx.layoutStyle(hierarchyTree);
        layout.minSize = Vec2{0.0f, 120.0f};
        layout.preferredSize = Vec2{0.0f, 0.0f};
    }

    void HierarchyPanel::rebuildHierarchy(Context &ctx, Scene::Scene &scene, std::optional<Scene::EntityId> restoreSelection)
    {
        std::vector<std::pair<Scene::EntityId, bool>> expansionStates;
        if (!resetHierarchyExpansion)
        {
            expansionStates.reserve(hierarchyEntityItems.size());
            for (const auto &[id, item] : hierarchyEntityItems)
            {
                expansionStates.emplace_back(id, ctx.isTreeViewItemExpanded(hierarchyTree, item));
            }
        }

        ctx.clearTreeView(hierarchyTree);
        hierarchyEntityTokens.clear();
        hierarchyEntityItems.clear();
        nextHierarchyToken = 1;

        TreeViewItemId restoredItem{};
        const auto addChildren = [&](auto &&self, std::optional<Scene::EntityId> parentId, TreeViewItemId parentItem) -> bool {
            bool containsRestoreSelection = false;
            const std::vector<Scene::EntityId> children = parentId ? scene.getChildren(*parentId) : scene.getRootEntities();

            for (const Scene::EntityId id : children)
            {
                const auto entity = scene.findEntity(id);
                if (!entity)
                {
                    continue;
                }

                const auto *name = entity->try_get<Scene::NameComponent>();
                const std::string label = name ? name->name : "Entity";
                const std::uint64_t token = nextHierarchyToken++;
                const TreeViewItemId item = ctx.addTreeViewItem(hierarchyTree, parentItem, label, token);

                hierarchyEntityTokens.emplace_back(token, id);
                hierarchyEntityItems.emplace_back(id, item);

                const auto previousExpansion = std::find_if(expansionStates.begin(), expansionStates.end(), [id](const auto &entry) { return entry.first == id; });

                if (resetHierarchyExpansion)
                {
                    ctx.setTreeViewItemExpanded(hierarchyTree, item, false);
                }
                else if (previousExpansion != expansionStates.end())
                {
                    ctx.setTreeViewItemExpanded(hierarchyTree, item, previousExpansion->second);
                }

                const bool isRestoreTarget = restoreSelection && id == *restoreSelection;
                if (isRestoreTarget)
                {
                    restoredItem = item;
                }

                const bool hasRestoredDescendant = self(self, id, item);
                if (hasRestoredDescendant)
                {
                    ctx.setTreeViewItemExpanded(hierarchyTree, item, true);
                }

                containsRestoreSelection = containsRestoreSelection || isRestoreTarget || hasRestoredDescendant;
            }

            return containsRestoreSelection;
        };

        addChildren(addChildren, std::nullopt, {});
        resetHierarchyExpansion = false;
        selectedEntity_.reset();
        if (static_cast<bool>(restoredItem))
        {
            ctx.setTreeViewSelectedItem(hierarchyTree, restoredItem);
            selectedEntity_ = restoreSelection;
        }
    }

    void HierarchyPanel::updateSelection(Context &ctx)
    {
        const TreeViewItemId item = ctx.treeViewSelectedItem(hierarchyTree);
        if (!static_cast<bool>(item))
        {
            selectedEntity_.reset();
            return;
        }

        const std::uint64_t token = ctx.treeViewItemUserValue(hierarchyTree, item);
        for (const auto &[candidateToken, id] : hierarchyEntityTokens)
        {
            if (candidateToken == token)
            {
                selectedEntity_ = id;
                return;
            }
        }

        selectedEntity_.reset();
    }

    void HierarchyPanel::selectEntity(Context &ctx, std::optional<Scene::EntityId> entity)
    {
        if (entity)
        {
            for (const auto &[id, item] : hierarchyEntityItems)
            {
                if (id == *entity)
                {
                    ctx.setTreeViewSelectedItem(hierarchyTree, item);
                    selectedEntity_ = entity;
                    return;
                }
            }
        }

        ctx.setTreeViewSelectedItem(hierarchyTree, {});
        selectedEntity_.reset();
    }

    void HierarchyPanel::update(Context &ctx, Engine &engine, Scribe::Session &session)
    {
        auto closeHierarchyRename = [&]()
        {
            if (!ctx.isValid(hierarchyRenameInput))
            {
                return;
            }

            ctx.destroyNode(hierarchyRenameInput);
            hierarchyRenameInput = {};
            hierarchyRenameEntity.reset();
            hierarchyRenameItem = {};

            ctx.layoutStyle(hierarchyTree).mode = hierarchyTreeLayoutModeBeforeRename;
            ctx.layoutStyle(hierarchyTree).clip = hierarchyTreeClipBeforeRename;
        };

        hierarchyClickAge = std::min(1.0f, hierarchyClickAge + ctx.input().deltaTime);
        Scene::Scene *activeScene = engine.getActiveScene();

        if (!engine.isPlaying() && !ctx.isValid(hierarchyRenameInput) && ctx.isFocused(hierarchyTree))
        {
            Context::TreeViewDrop drop{};
            while (ctx.pollTreeViewDrop(hierarchyTree, drop))
            {
                auto findEntityForItem = [&](TreeViewItemId item) -> std::optional<Scene::EntityId>
                {
                    if (!item)
                    {
                        return std::nullopt;
                    }

                    const std::uint64_t token = ctx.treeViewItemUserValue(hierarchyTree, item);
                    for (const auto &[candidateToken, entityId] : hierarchyEntityTokens)
                    {
                        if (candidateToken == token)
                        {
                            return entityId;
                        }
                    }
                    return std::nullopt;
                };

                const auto draggedEntity = findEntityForItem(drop.dragged);
                if (!draggedEntity || (drop.target && drop.dragged == drop.target))
                {
                    continue;
                }

                std::optional<Scene::EntityId> newParent;
                std::optional<std::size_t> siblingIndex;

                if (!drop.target)
                {
                    newParent = std::nullopt;
                }
                else
                {
                    const auto targetEntity = findEntityForItem(drop.target);
                    if (!targetEntity || !activeScene)
                    {
                        continue;
                    }

                    bool isDescendant = false;
                    for (auto current = targetEntity; current; current = activeScene->getParent(*current))
                    {
                        if (*current == *draggedEntity)
                        {
                            isDescendant = true;
                            break;
                        }
                    }
                    if (isDescendant)
                    {
                        continue;
                    }

                    if (drop.pos == TreeViewDropPosition::Inside)
                    {
                        newParent = *targetEntity;
                    }
                    else
                    {
                        newParent = activeScene->getParent(*targetEntity);
                        const std::vector<Scene::EntityId> siblings = newParent
                            ? activeScene->getChildren(*newParent)
                            : activeScene->getRootEntities();
                        const auto targetIt = std::find(siblings.begin(), siblings.end(), *targetEntity);
                        if (targetIt == siblings.end())
                        {
                            continue;
                        }

                        std::size_t index = static_cast<std::size_t>(targetIt - siblings.begin());
                        if (drop.pos == TreeViewDropPosition::After)
                        {
                            ++index;
                        }
                        siblingIndex = index;
                    }
                }

                session.endMerge();
                const auto result = session.reparentEntity(*draggedEntity, newParent, siblingIndex);
                session.endMerge();
                if (!result.ok())
                {
                    if (result.status != Scribe::Result::Status::NoChange)
                    {
                        Log::Print("REPARENT FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
                    }
                    continue;
                }

                selectedEntity_ = *draggedEntity;
                break;
            }
        }

        if (!activeScene)
        {
            if (!engine.createEmptyScene("Untitled"))
            {
                Log::Print("FAILED TO CREATE FALLBACK SCENE", "Editor", LogType::LOG_ERROR);
                return;
            }
            activeScene = engine.getActiveScene();
        }

        bool hierarchyDirty = false;
        for (const Scribe::ChangeEvent &event : session.takeEvents())
        {
            if (event.type == Scribe::ChangeType::SceneReplaced)
            {
                selectedEntity_.reset();
                hierarchyDirty = true;
                resetHierarchyExpansion = true;
            }
            else if (event.type == Scribe::ChangeType::EntityDestroyed)
            {
                if (selectedEntity_ && event.entityId == *selectedEntity_)
                {
                    selectedEntity_.reset();
                }
                hierarchyDirty = true;
            }
            else if (event.type == Scribe::ChangeType::EntityCreated || event.type == Scribe::ChangeType::EntityRenamed || event.type == Scribe::ChangeType::EntityReparented)
            {
                hierarchyDirty = true;
            }
        }

        if (hierarchyDirty)
        {
            closeHierarchyRename();
            lastHierarchyClickEntity.reset();
            hierarchyClickAge = 1.0f;
        }

        if (!activeScene)
        {
            if (hierarchyInitialized)
            {
                closeHierarchyRename();
                ctx.clearTreeView(hierarchyTree);
                hierarchyEntityTokens.clear();
                hierarchyEntityItems.clear();
                selectedEntity_.reset();
                hierarchyInitialized = false;
            }
        }
        else
        {
            const std::uint64_t sceneGeneration = engine.getSceneGeneration();

            const bool hierarchyNeedsRebuild = !hierarchyInitialized || sceneGeneration != hierarchySceneGeneration ||
                activeScene->getEntityCount() != hierarchyEntityTokens.size() || hierarchyDirty;
            if (hierarchyNeedsRebuild)
            {
                closeHierarchyRename();
                lastHierarchyClickEntity.reset();
                hierarchyClickAge = 0;

                rebuildHierarchy(ctx, *activeScene, selectedEntity_);
                hierarchySceneGeneration = sceneGeneration;
                hierarchyInitialized = true;
            }
            updateSelection(ctx);
        }

        bool f2Pressed = false;
        bool enterPressed = false;
        bool escapePressed = false;
        bool deletePressed = false;

        for (const InputEvent &event : ctx.input().events)
        {
            if (event.type != InputEventType::Key || event.action != KeyAction::Press)
            {
                continue;
            }

            f2Pressed = f2Pressed || event.key == Key::F2;
            enterPressed = enterPressed || event.key == Key::Enter || event.key == Key::KeypadEnter;
            escapePressed = escapePressed || event.key == Key::Escape;
            deletePressed = deletePressed || event.key == Key::Delete;
        }

        bool renameRequested = f2Pressed && ctx.isFocused(hierarchyTree);
        if (!engine.isPlaying() && !ctx.isValid(hierarchyRenameInput) && ctx.input().mouse.released(MouseButton::Left))
        {
            const TreeViewState *treeState = ctx.tree().treeViewState(hierarchyTree);
            const TreeViewItemId hoveredItem = treeState ? treeState->hovered : TreeViewItemId{};
            const TreeViewItemId selectedItem = ctx.treeViewSelectedItem(hierarchyTree);

            Rect textRect{};
            std::optional<Scene::EntityId> clickedEntity;

            if (ctx.isHovered(hierarchyTree) &&
                hoveredItem &&
                hoveredItem == selectedItem &&
                hierarchyItemTextRect(ctx, hierarchyTree, hoveredItem, textRect) &&
                ctx.input().mouse.position.x >= textRect.x && ctx.input().mouse.position.x <= textRect.maxX())
            {
                const std::uint64_t token = ctx.treeViewItemUserValue(hierarchyTree, hoveredItem);

                for (const auto &[candidateToken, id] : hierarchyEntityTokens)
                {
                    if (candidateToken == token)
                    {
                        clickedEntity = id;
                        break;
                    }
                }
            }

            if (clickedEntity && lastHierarchyClickEntity == clickedEntity && hierarchyClickAge <= 0.35f)
            {
                renameRequested = true;
                lastHierarchyClickEntity.reset();
                hierarchyClickAge = 1.0f;
            }
            else if (clickedEntity)
            {
                lastHierarchyClickEntity = clickedEntity;
                hierarchyClickAge = 0.0f;
            }
            else
            {
                lastHierarchyClickEntity.reset();
                hierarchyClickAge = 1.0f;
            }
        }

        auto beginHierarchyRename = [&]()
        {
            if (!activeScene || !selectedEntity_)
            {
                return;
            }

            const TreeViewItemId item = ctx.treeViewSelectedItem(hierarchyTree);
            Rect textRect{};
            if (!item || !hierarchyItemTextRect(ctx, hierarchyTree, item, textRect))
            {
                return;
            }

            const std::string currentName(ctx.treeViewItemText(hierarchyTree, item));
            if (currentName.empty())
            {
                return;
            }

            const LayoutStyle *treeLayout = ctx.tree().layoutStyle(hierarchyTree);
            hierarchyTreeLayoutModeBeforeRename = treeLayout ? treeLayout->mode : LayoutMode::Vertical;
            hierarchyTreeClipBeforeRename = treeLayout && treeLayout->clip;

            ctx.layoutStyle(hierarchyTree).mode = LayoutMode::Manual;
            ctx.layoutStyle(hierarchyTree).clip = true;

            hierarchyRenameInput = ctx.createTextInput(hierarchyTree, currentName);
            if (!ctx.isValid(hierarchyRenameInput))
            {
                ctx.layoutStyle(hierarchyTree).mode = hierarchyTreeLayoutModeBeforeRename;
                ctx.layoutStyle(hierarchyTree).clip = hierarchyTreeClipBeforeRename;
                return;
            }

            hierarchyRenameEntity = selectedEntity_;
            hierarchyRenameItem = item;

            auto inputLayout = ctx.layoutStyle(hierarchyRenameInput);
            inputLayout.padding = Padding{3.0f, 1.0f};
            inputLayout.minSize = Vec2{0.0f, HierarchyRowHeight};
            inputLayout.preferredSize = Vec2{0.0f, HierarchyRowHeight};
            inputLayout.maxSize = Vec2{InfiniteLayoutSize, HierarchyRowHeight};

            if (TextInputState *inputState = ctx.tree().textInputState(hierarchyRenameInput))
            {
                inputState->selectionAnchor = 0;
                inputState->cursor = static_cast<u32>(ctx.text(hierarchyRenameInput).size());
            }

            ctx.tree().setRect(hierarchyRenameInput, textRect);
            ctx.tree().markPaintDirty(hierarchyRenameInput);
            ctx.setFocused(hierarchyRenameInput);

            lastHierarchyClickEntity.reset();
            hierarchyClickAge = 1.0f;
        };

        auto finishHierarchyRename = [&](bool commit)
        {
            if (!ctx.isValid(hierarchyRenameInput))
            {
                return;
            }

            const auto entityId = hierarchyRenameEntity;
            const std::string enteredName(ctx.text(hierarchyRenameInput));

            if (commit && activeScene && entityId)
            {
                const std::string trimmedName = trimEntityName(enteredName);
                if (!trimmedName.empty())
                {
                    const std::string finalName = uniqueEntityName(*activeScene, trimmedName, entityId);

                    session.endMerge();
                    const auto result = session.renameEntity(*entityId, finalName);
                    session.endMerge();

                    if (!result.ok())
                    {
                        Log::Print("RENAME ENTITY FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
                    }
                }
            }

            closeHierarchyRename();
        };

        if (renameRequested && !engine.isPlaying() && activeScene && selectedEntity_ && !ctx.isValid(hierarchyRenameInput) && ctx.isFocused(hierarchyTree))
        {
            beginHierarchyRename();
        }

        if (ctx.isValid(hierarchyRenameInput))
        {
            Rect textRect{};
            if (hierarchyItemTextRect(ctx, hierarchyTree, hierarchyRenameItem, textRect))
            {
                ctx.tree().setRect(hierarchyRenameInput, textRect);
                ctx.tree().markPaintDirty(hierarchyRenameInput);
            }

            if (engine.isPlaying())
            {
                closeHierarchyRename();
            }
            else if (escapePressed)
            {
                finishHierarchyRename(false);
            }
            else if (enterPressed)
            {
                finishHierarchyRename(true);
            }
            else if (!ctx.isFocused(hierarchyRenameInput))
            {
                finishHierarchyRename(true);
            }
        }

        const NodeHandle focused = ctx.focused();
        const bool textFieldFocused =
            ctx.isValid(focused) &&
            (ctx.tree().type(focused) == NodeType::TextInput ||
             ctx.tree().type(focused) == NodeType::NumericInput);

        if (deletePressed &&
            !engine.isPlaying() && activeScene && ctx.isFocused(hierarchyTree) && !textFieldFocused && !ctx.isValid(hierarchyRenameInput) && selectedEntity_)
        {
            const Scene::EntityId entityId = *selectedEntity_;

            session.endMerge();
            const auto result = session.deleteEntity(entityId);
            session.endMerge();

            if (result.ok())
            {
                selectedEntity_.reset();
                ctx.setTreeViewSelectedItem(hierarchyTree, TreeViewItemId{});
            }
            else
            {
                Log::Print("DELETE ENTITY FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
            }
        }

        if (ctx.isClicked(createEntityButton) && !engine.isPlaying())
        {
            Scene::Scene *scene = engine.getActiveScene();
            if (scene)
            {
                const std::string name = uniqueEntityName(*scene, "Entity");
                const auto created = session.createEntity(name, selectedEntity_);

                if (created.ok())
                {
                    selectedEntity_ = created.id;
                }
                else
                {
                    Log::Print("CREATE ENTITY FAILED: " + created.result.error, "Editor", LogType::LOG_ERROR);
                }
            }
        }

        const bool playing = engine.isPlaying();
        ctx.setEnabled(createEntityButton, !playing);
        ctx.setIconColor(createEntityIcon, playing ? ctx.theme().textDisabled : ctx.theme().textMuted);
    }
}