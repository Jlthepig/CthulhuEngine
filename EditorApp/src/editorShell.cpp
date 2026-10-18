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

        NodeHandle panelHeader(Context &ctx, NodeHandle panel, std::string_view icon,std::string_view title)
        {
            const NodeHandle header = ctx.createContainer(panel);

            auto layout = ctx.layoutStyle(header);
            layout.mode = LayoutMode::Horizontal;
            layout.gap = 6.0f;
            layout.verticalAlignment = Alignment::Center;
            layout.minSize = Vec2{0.0f, 20.0f};
            layout.preferredSize = Vec2{0.0f, 20.0f};
            layout.maxSize = Vec2{InfiniteLayoutSize, 20.0f};

            const NodeHandle headerIcon = ctx.createIcon(header, icon);
            ctx.setIconSize(headerIcon, 16.0f);
            ctx.setIconColor(headerIcon, ctx.theme().textMuted);

            const NodeHandle heading = ctx.createLabel(header, title);
            ctx.style(heading).all.text = ctx.theme().textMuted;

            return header;
        }

        NodeHandle titledPanel(Context &ctx, NodeHandle pane, std::string_view icon,std::string_view title, NodeHandle *outHeader = nullptr)
        {
            fillWithChild(ctx, pane);

            const NodeHandle panel = ctx.createPanel(pane);

            auto layout = ctx.layoutStyle(panel);
            layout.mode = LayoutMode::Vertical;
            layout.horizontalAlignment = Alignment::Stretch;
            layout.verticalAlignment = Alignment::Stretch;
            layout.gap = 6.0f;
            layout.padding = Padding{8.0f};

            const NodeHandle header = panelHeader(ctx, panel, icon, title);
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

        playButton = iconButton(ctx, topBar, "play");
        pauseButton = iconButton(ctx, topBar, "pause");
        stepButton = iconButton(ctx, topBar, "chevron-right");
        styleToggle(ctx, playButton, false);
        styleToggle(ctx, pauseButton, false);
        ctx.setIconColor(stepButton.icon, ctx.theme().text);
        ctx.setEnabled(pauseButton.button, false);
        ctx.setEnabled(stepButton.button, false);
        [[maybe_unused]] const NodeHandle trailingSpacer = ctx.createContainer(topBar);

        // hierachy << scene << inspector
         const NodeHandle workspace =
        ctx.createSplitContainer(root, SplitOrientation::Horizontal, 0.14f);
        ctx.setSplitMinimumSizes(workspace, 140.0f, 600.0f);
        seeThrough(ctx, workspace);
        seeThrough(ctx, ctx.splitSecondPane(workspace));
        hierarchy = titledPanel(ctx, ctx.splitFirstPane(workspace), "sidebar-left", "HIERARCHY", &hierarchyHeader);

        hierarchyTree = ctx.createTreeView(hierarchy);
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

    void EditorShell::rebuildHierarchy(Context &ctx, Scene::Scene &scene,
                                      std::optional<Scene::EntityId> restoreSelection)
    {
        ctx.clearTreeView(hierarchyTree);
        hierarchyEntityTokens.clear();
        nextHierarchyToken = 1;

        octogui::TreeViewItemId restoredItem{};
        const auto addChildren = [&](auto &&self, std::optional<Scene::EntityId> parentId,
                                     octogui::TreeViewItemId parentItem) -> void {
            const std::vector<Scene::EntityId> children =
                parentId ? scene.getChildren(*parentId) : scene.getRootEntities();

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
                const octogui::TreeViewItemId item =
                    ctx.addTreeViewItem(hierarchyTree, parentItem, label, token);

                hierarchyEntityTokens.emplace_back(token, id);
                if (restoreSelection && id == *restoreSelection)
                {
                    restoredItem = item;
                }

                self(self, id, item);
            }
        };

        addChildren(addChildren, std::nullopt, {});
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
        bool hierarchyDirty = false;
        for (const Scribe::ChangeEvent &event : session.takeEvents())
        {
            if (event.type == Scribe::ChangeType::EntityCreated ||
                event.type == Scribe::ChangeType::EntityDestroyed ||
                event.type == Scribe::ChangeType::EntityRenamed ||
                event.type == Scribe::ChangeType::EntityReparented)
            {
                hierarchyDirty = true;
            }
        }

        Scene::Scene *activeScene = engine.getActiveScene();
        if (!activeScene)
        {
            if (hierarchyInitialized)
            {
                ctx.clearTreeView(hierarchyTree);
                hierarchyEntityTokens.clear();
                selectedEntity.reset();
                hierarchyInitialized = false;
            }
        }
        else
        {
            const std::uint64_t sceneGeneration = engine.getSceneGeneration();
            if (!hierarchyInitialized || sceneGeneration != hierarchySceneGeneration ||
                activeScene->getEntityCount() != hierarchyEntityTokens.size() || hierarchyDirty)
            {
                rebuildHierarchy(ctx, *activeScene, selectedEntity);
                hierarchySceneGeneration = sceneGeneration;
                hierarchyInitialized = true;
            }
            updateSelection(ctx);
        }

        if (ctx.isClicked(saveButton))
        {
            const auto result = session.save();
            if (!result.ok())
            {
                Log::Print("SAVE FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
            }
        }

        if (ctx.isClicked(playButton.button))
        {
            const bool ok = engine.isPlaying() ? engine.stop() : engine.play();
            (void)ok;
        }

        if (ctx.isClicked(pauseButton.button) && engine.isPlaying())
        {
            const bool paused = engine.getSimulationState() == SimulationState::Paused;
            engine.setSimulationState(paused ? SimulationState::Running : SimulationState::Paused);
        }

        if (ctx.isClicked(stepButton.button) && engine.isPlaying() &&
            engine.getSimulationState() != SimulationState::Running)
        {
            engine.stepSimulation();
        }

        const bool playing = engine.isPlaying();
        const bool paused = playing && engine.getSimulationState() != SimulationState::Running;

        if (playing != lastPlaying)
        {
            ctx.setIcon(playButton.icon, playing ? "stop" : "play");
            styleToggle(ctx, playButton, playing);
            ctx.setEnabled(saveButton, !playing);
            ctx.setEnabled(pauseButton.button, playing);
            lastPlaying = playing;
        }

        if (paused != lastPaused)
        {
            styleToggle(ctx, pauseButton, paused);
            ctx.setEnabled(stepButton.button, paused);
            lastPaused = paused;
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
