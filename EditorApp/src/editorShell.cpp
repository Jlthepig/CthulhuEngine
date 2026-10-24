#include <cstddef>
#include <string>
#include <algorithm>
#include <cctype>

#include "editorShell.hpp"

#include "engine.hpp"
#include "components.hpp"
#include "scene.hpp"
#include "log_utils.hpp"
#include "session.hpp"

using namespace octogui;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
    namespace
    {
        void fillWithChild(Context & ctx, NodeHandle pane)
        {
            auto layout = ctx.layoutStyle(pane);
            layout.mode = LayoutMode::Stack;
            layout.horizontalAlignment = Alignment::Stretch;
            layout.verticalAlignment = Alignment::Stretch;
        }

        NodeHandle panelHeader(Context &ctx, NodeHandle panel, std::string_view icon,std::string_view title, IconButton *outLeadingAction = nullptr)
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

        NodeHandle titledPanel(Context &ctx, NodeHandle pane, std::string_view icon,std::string_view title, NodeHandle *outHeader = nullptr, IconButton *outLeadingAction = nullptr)
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

        void seeThrough(Context &ctx, NodeHandle node)
        {
            ctx.style(node).all.background = Color::transparent();
            ctx.style(node).shadow = ShadowStyle{};
        }

        void fixedSize(octogui::Context &ctx, octogui::NodeHandle node, octogui::f32 width, octogui::f32 height)
        {
            auto layout = ctx.layoutStyle(node);
            layout.minSize = octogui::Vec2{width, height};
            layout.preferredSize = octogui::Vec2{width, height};
        }

        void verticalSeparator(octogui::Context &ctx, octogui::NodeHandle parent, octogui::f32 height)
        {
            const octogui::NodeHandle separator = ctx.createSeparator(parent);
            ctx.setSeparatorOrientation(separator, octogui::SeparatorOrientation::Vertical);
            fixedSize(ctx, separator, 1.0f, height);
        }

        IconButton iconButton(octogui::Context &ctx, octogui::NodeHandle parent, std::string_view icon)
        {
            IconButton result;
            result.button = ctx.createButton(parent, "");
            fixedSize(ctx, result.button, 32.0f, 26.0f);

            auto layout = ctx.layoutStyle(result.button);
            layout.mode = octogui::LayoutMode::Horizontal;
            layout.padding = octogui::Padding{};
            layout.horizontalAlignment = octogui::Alignment::Center;
            layout.verticalAlignment = octogui::Alignment::Center;

            result.icon = ctx.createIcon(result.button, icon);
            ctx.setIconSize(result.icon, 18.0f);
            return result;
        }

        void styleToggle(octogui::Context &ctx, const IconButton &toggle, bool on)
        {
            const octogui::Theme &theme = ctx.theme();
            auto style = ctx.style(toggle.button);
            style.normal.background = on ? theme.accent : theme.surface;
            style.normal.border = on ? theme.accentActive : theme.border;
            ctx.setIconColor(toggle.icon, on ? theme.accentText : theme.text);
        }

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
                return found && (!ignoredEntity || *found != * ignoredEntity);
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

        void fixedBar(Context &ctx, NodeHandle bar, f32 height)
        {
            auto layout = ctx.layoutStyle(bar);
            layout.mode = LayoutMode::Horizontal;
            layout.verticalAlignment = Alignment::Center;
            layout.gap = 6.0f;
            layout.padding = Padding{8.0f, 2.0f};
            layout.minSize = Vec2{0.0f, height};
            layout.preferredSize = Vec2{0.0f, height};
        }

    } // namespace

    void EditorShell::build(Context &ctx)
    {
        const NodeHandle root = ctx.ensureRoot();
        seeThrough(ctx, root);
        auto rootLayout = ctx.layoutStyle(root);
        rootLayout.mode = LayoutMode::Vertical;
        rootLayout.horizontalAlignment = Alignment::Stretch;

        const octogui::NodeHandle topBar = ctx.createPanel(root);
        fixedBar(ctx, topBar, 50.0f);
        auto topLayout = ctx.layoutStyle(topBar);
        topLayout.gap = 8.0f;
        topLayout.padding = octogui::Padding{12.0f, 2.0f};
        topLayout.horizontalAlignment = Alignment::Start;

        const octogui::NodeHandle logo = ctx.createIcon(topBar, "Cthulhu");
        ctx.setIconSize(logo, 46.0f);

        sceneLabel = ctx.createLabel(topBar, "");
        ctx.style(sceneLabel).all.text = ctx.theme().textMuted;
        fixedSize(ctx, sceneLabel, 150.0f, 28.0f);
        verticalSeparator(ctx, topBar, 24.0f);

        saveButton = ctx.createButton(topBar, "Save");
        fixedSize(ctx, saveButton, 68.0f, 26.0f);
        verticalSeparator(ctx, topBar, 24.0f);

        [[maybe_unused]] const NodeHandle controlsSpacer = ctx.createContainer(topBar);

        playButton = iconButton(ctx, topBar, "play");
        pauseButton = iconButton(ctx, topBar, "pause");
        stopButton = iconButton(ctx, topBar, "stop");
        styleToggle(ctx, playButton, false);
        styleToggle(ctx, pauseButton, false);
        ctx.setEnabled(pauseButton.button, false);
        ctx.setEnabled(stopButton.button, false);
        ctx.setIconColor(pauseButton.icon, ctx.theme().textDisabled);
        ctx.setIconColor(stopButton.icon, ctx.theme().textDisabled);

        // hierachy << scene << inspector
        const NodeHandle workspace =
        ctx.createSplitContainer(root, SplitOrientation::Horizontal, 0.14f);
        ctx.setSplitMinimumSizes(workspace, 180.0f, 600.0f);
        seeThrough(ctx, workspace);
        seeThrough(ctx, ctx.splitSecondPane(workspace));
        hierarchy = titledPanel(ctx, ctx.splitFirstPane(workspace), "sidebar-left", "HIERARCHY", &hierarchyHeader, &createEntityButton);

        hierarchyTree = ctx.createTreeView(hierarchy);
        ctx.setTreeViewMaxIndentWidth(hierarchyTree, HierarchyIndentCap);
        auto treeLayout = ctx.layoutStyle(hierarchyTree);
        treeLayout.minSize = Vec2{0.0f, 120.0f};
        treeLayout.preferredSize = Vec2{0.0f, 0.0f};

        const NodeHandle sceneInspector =
        ctx.createSplitContainer(ctx.splitSecondPane(workspace), SplitOrientation::Horizontal, 0.78f);
        ctx.setSplitMinimumSizes(sceneInspector, 260.0f, 280.0f);
        seeThrough(ctx, sceneInspector);

        sceneNode = ctx.splitFirstPane(sceneInspector);
        seeThrough(ctx, sceneNode);

        ctx.style(sceneNode).all.background = Color::transparent();

        inspector = titledPanel(ctx, ctx.splitSecondPane(sceneInspector), "sliders", "INSPECTOR");
        inspectorUI.build(ctx, inspector);

        const NodeHandle statusBar = ctx.createPanel(root);
        fixedBar(ctx, statusBar, 24.0f);
    }

    void EditorShell::rebuildHierarchy(Context &ctx, Scene::Scene &scene, std::optional<Scene::EntityId> restoreSelection)
    {
        std::vector<std::pair<Scene::EntityId, bool>> expansionStates;
        if (!resetHierarchyExpansion)
        {
            expansionStates.reserve(hierarchyEntityItems.size());
            for (const auto &[id,item] : hierarchyEntityItems)
            {
                expansionStates.emplace_back(id, ctx.isTreeViewItemExpanded(hierarchyTree, item));
            }
        }

        ctx.clearTreeView(hierarchyTree);
        hierarchyEntityTokens.clear();
        hierarchyEntityItems.clear();
        nextHierarchyToken = 1;

        octogui::TreeViewItemId restoredItem{};
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
                hierarchyEntityItems.emplace_back(id,item);

                const auto previousExpansion = std::find_if(expansionStates.begin(), expansionStates.end(), [id](const auto &entry) {return entry.first == id; });

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

                const bool hasRestoredDescendant = self(self,id, item);
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
        selectedEntity.reset();
        if (static_cast<bool>(restoredItem))
        {
            ctx.setTreeViewSelectedItem(hierarchyTree, restoredItem);
            selectedEntity = restoreSelection;
        }
    }

    void EditorShell::updateSelection(Context &ctx)
    {
        const octogui::TreeViewItemId item = ctx.treeViewSelectedItem(hierarchyTree);
        if (!static_cast<bool>(item))
        {
            selectedEntity.reset();
            return;
        }

        const std::uint64_t token = ctx.treeViewItemUserValue(hierarchyTree, item);
        for (const auto &[candidateToken, id] : hierarchyEntityTokens)
        {
            if (candidateToken == token)
            {
                selectedEntity = id;
                return;
            }
        }

        selectedEntity.reset();
    }

    void EditorShell::update(Context &ctx, Engine &engine, Scribe::Session &session)
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

        hierarchyClickAge = std::min(1.0f,hierarchyClickAge + ctx.input().deltaTime);

        if (!engine.getActiveScene() && !engine.createEmptyScene("Untitled"))
        {
            Log::Print("FAILED TO CREATE FALLBACK SCENE", "Editor", LogType::LOG_ERROR);
            return;
        }

        bool hierarchyDirty = false;
        for (const Scribe::ChangeEvent &event : session.takeEvents())
        {
            if (event.type == Scribe::ChangeType::SceneReplaced)
            {
                selectedEntity.reset();
                hierarchyDirty = true;
                resetHierarchyExpansion = true;
            }
            else if (event.type == Scribe::ChangeType::EntityDestroyed)
            {
                if (selectedEntity && event.entityId == *selectedEntity)
                {
                    selectedEntity.reset();
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

        Scene::Scene *activeScene = engine.getActiveScene();
        if (!activeScene)
        {
            if (hierarchyInitialized)
            {
                closeHierarchyRename();
                ctx.clearTreeView(hierarchyTree);
                hierarchyEntityTokens.clear();
                                hierarchyEntityItems.clear();
                selectedEntity.reset();
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

                rebuildHierarchy(ctx, *activeScene, selectedEntity);
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
            if (!activeScene || !selectedEntity)
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

            hierarchyRenameEntity = selectedEntity;
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
                        Log::Print("RENAME ENTITY FAILED: " + result.error,"Editor",LogType::LOG_ERROR);
                    }
                }
            }

            closeHierarchyRename();
        };

        if (renameRequested && !engine.isPlaying() && activeScene && selectedEntity && !ctx.isValid(hierarchyRenameInput) && ctx.isFocused(hierarchyTree))
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
            !engine.isPlaying() && activeScene && ctx.isFocused(hierarchyTree) && !textFieldFocused && !ctx.isValid(hierarchyRenameInput) && selectedEntity)
        {
            const Scene::EntityId entityId = *selectedEntity;

            session.endMerge();
            const auto result = session.deleteEntity(entityId);
            session.endMerge();

            if (result.ok())
            {
                selectedEntity.reset();
                ctx.setTreeViewSelectedItem(hierarchyTree, TreeViewItemId{});
            }
            else
            {
                Log::Print("DELETE ENTITY FAILED: " + result.error,"Editor",LogType::LOG_ERROR);
            }
        }

        if (ctx.isClicked(createEntityButton.button) && !engine.isPlaying())
        {
            Scene::Scene *scene = engine.getActiveScene();
            if (scene)
            {
                const std::string name = uniqueEntityName(*scene, "Entity");
                const auto created = session.createEntity(name, selectedEntity);

                if (created.ok())
                {
                    selectedEntity = created.id;
                }
                else
                {
                    Log::Print("CREATE ENTITY FAILED: " + created.result.error,"Editor", LogType::LOG_ERROR);
                }
            }
        }

        if (ctx.isClicked(saveButton))
        {
            const auto result = session.save();
            if (!result.ok())
            {
                Log::Print("SAVE FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
            }
        }

        if (ctx.isClicked(playButton.button) && !engine.isPlaying())
        {
            const bool ok = engine.play();
            (void)ok;
        }

        if (ctx.isClicked(pauseButton.button) && engine.isPlaying())
        {
            const bool paused = engine.getSimulationState() == SimulationState::Paused;
            engine.setSimulationState(paused ? SimulationState::Running : SimulationState::Paused);
        }

        if (ctx.isClicked(stopButton.button) && engine.isPlaying())
        {
            const bool ok = engine.stop();
            (void)ok;
        }

        const bool playing = engine.isPlaying();
        const bool paused = playing && engine.getSimulationState() != SimulationState::Running;

        if (paused != lastPaused)
        {
            styleToggle(ctx, pauseButton, paused);
            lastPaused = paused;
        }

        if (playing != lastPlaying)
        {
            ctx.setEnabled(saveButton, !playing);
            ctx.setEnabled(playButton.button, !playing);
            ctx.setEnabled(pauseButton.button, playing);
            ctx.setEnabled(stopButton.button, playing);
            ctx.setEnabled(createEntityButton.button, !playing);

            ctx.setIconColor(playButton.icon,playing ? ctx.theme().textDisabled : ctx.theme().text);
            ctx.setIconColor(pauseButton.icon, playing ? ctx.theme().text : ctx.theme().textDisabled);
            ctx.setIconColor(stopButton.icon,playing ? ctx.theme().text : ctx.theme().textDisabled);
            ctx.setIconColor(createEntityButton.icon, playing ? ctx.theme().textDisabled : ctx.theme().textMuted);

            lastPlaying = playing;
        }

        inspectorUI.update(ctx, engine, session, engine.getActiveScene(), selectedEntity);

        std::string sceneText = "No scene";
        if (const auto *activeScene = engine.getActiveScene())
        {
            sceneText = activeScene->getName() + (activeScene->isDirty() ? " *" : "");
        }

        if (sceneText != lastSceneText)
        {
            ctx.setText(sceneLabel, sceneText);
            lastSceneText = sceneText;
        }

    }
}
