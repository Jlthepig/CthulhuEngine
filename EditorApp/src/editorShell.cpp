#include <string>

#include "editorShell.hpp"

#include "engine.hpp"
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

        NodeHandle titledPanel(Context &ctx, NodeHandle pane, std::string_view icon, std::string_view title, NodeHandle *outHeader = nullptr)
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

        void fixedSize(Context &ctx, NodeHandle node, f32 width, f32 height)
        {
            auto layout = ctx.layoutStyle(node);
            layout.minSize = Vec2{width, height};
            layout.preferredSize = Vec2{width, height};
        }

        void verticalSeparator(Context &ctx, NodeHandle parent, f32 height)
        {
            const NodeHandle separator = ctx.createSeparator(parent);
            ctx.setSeparatorOrientation(separator, SeparatorOrientation::Vertical);
            fixedSize(ctx, separator, 1.0f, height);
        }

        IconButton iconButton(Context &ctx, NodeHandle parent, std::string_view icon)
        {
            IconButton result;
            result.button = ctx.createButton(parent, "");
            fixedSize(ctx, result.button, 32.0f, 26.0f);

            auto layout = ctx.layoutStyle(result.button);
            layout.mode = LayoutMode::Horizontal;
            layout.padding = Padding{};
            layout.horizontalAlignment = Alignment::Center;
            layout.verticalAlignment = Alignment::Center;

            result.icon = ctx.createIcon(result.button, icon);
            ctx.setIconSize(result.icon, 18.0f);
            return result;
        }

        void styleToggle(Context &ctx, const IconButton &toggle, bool on)
        {
            const Theme &theme = ctx.theme();
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
    }

    void EditorShell::build(Context &ctx)
    {
        const NodeHandle root = ctx.ensureRoot();
        seeThrough(ctx, root);
        auto rootLayout = ctx.layoutStyle(root);
        rootLayout.mode = LayoutMode::Vertical;
        rootLayout.horizontalAlignment = Alignment::Stretch;

        const NodeHandle topBar = ctx.createPanel(root);
        fixedBar(ctx, topBar, 50.0f);
        auto topLayout = ctx.layoutStyle(topBar);
        topLayout.gap = 8.0f;
        topLayout.padding = Padding{12.0f, 2.0f};
        topLayout.horizontalAlignment = Alignment::Start;

        const NodeHandle logo = ctx.createIcon(topBar, "Cthulhu");
        ctx.setIconSize(logo, 46.0f);

        sceneLabel = ctx.createLabel(topBar, "");
        ctx.style(sceneLabel).all.text = ctx.theme().textMuted;
        fixedSize(ctx, sceneLabel, 150.0f, 28.0f);
        verticalSeparator(ctx, topBar, 24.0f);

        saveButton = iconButton(ctx, topBar, "save");
        ctx.layoutStyle(saveButton.button).gap = 6.0f;
        const NodeHandle saveLabel = ctx.createLabel(saveButton.button, "Save Scene");
        fixedSize(ctx, saveButton.button, 120.0f, 26.0f);
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

        const NodeHandle workspace = ctx.createSplitContainer(root, SplitOrientation::Horizontal, 0.14f);
        ctx.setSplitMinimumSizes(workspace, 180.0f, 600.0f);
        seeThrough(ctx, workspace);
        seeThrough(ctx, ctx.splitSecondPane(workspace));
        hierarchyPanelUI.build(ctx, ctx.splitFirstPane(workspace));

        const NodeHandle sceneInspector =
            ctx.createSplitContainer(ctx.splitSecondPane(workspace), SplitOrientation::Horizontal, 0.78f);
        ctx.setSplitMinimumSizes(sceneInspector, 260.0f, 280.0f);
        seeThrough(ctx, sceneInspector);

        sceneNode = ctx.splitFirstPane(sceneInspector);
        seeThrough(ctx, sceneNode);
        ctx.style(sceneNode).all.background = Color::transparent();
        {
            auto layout = ctx.layoutStyle(sceneNode);
            layout.mode = LayoutMode::Stack;
            layout.horizontalAlignment = Alignment::Stretch;
            layout.verticalAlignment = Alignment::Stretch;
            layout.padding = Padding{};
        }
        viewportPanelUI.build(ctx, sceneNode);

        inspector = titledPanel(ctx, ctx.splitSecondPane(sceneInspector), "sliders", "INSPECTOR");
        inspectorUI.build(ctx, inspector);

        const NodeHandle statusBar = ctx.createPanel(root);
        fixedBar(ctx, statusBar, 24.0f);
    }

    void EditorShell::update(Context &ctx, Engine &engine, Scribe::Session &session)
    {
        hierarchyPanelUI.update(ctx, engine, session);
        viewportPanelUI.update(ctx);

        if (ctx.isClicked(saveButton.button))
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
            ctx.setEnabled(saveButton.button, !playing);
            ctx.setEnabled(playButton.button, !playing);
            ctx.setEnabled(pauseButton.button, playing);
            ctx.setEnabled(stopButton.button, playing);

            ctx.setIconColor(playButton.icon, playing ? ctx.theme().textDisabled : ctx.theme().text);
            ctx.setIconColor(pauseButton.icon, playing ? ctx.theme().text : ctx.theme().textDisabled);
            ctx.setIconColor(stopButton.icon, playing ? ctx.theme().text : ctx.theme().textDisabled);

            lastPlaying = playing;
        }

        viewportPanelUI.updatePicking(ctx, engine, hierarchyPanelUI, sceneNode);
        inspectorUI.update(ctx, engine, session, engine.getActiveScene(), hierarchyPanelUI.selectedEntity());

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