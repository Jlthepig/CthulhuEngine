#include "editorShell.hpp"

#include "engine.hpp"
#include "log_utils.hpp"
#include "scene.hpp"
#include "session.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
    namespace
    {
        void fillWithChild(octogui::Context & ctx, octogui::NodeHandle pane)
        {
            auto layout = ctx.layoutStyle(pane);
            layout.mode = octogui::LayoutMode::Stack;
            layout.horizontalAlignment = octogui::Alignment::Stretch;
            layout.verticalAlignment = octogui::Alignment::Stretch;
        }

        octogui::NodeHandle titledPanel(octogui::Context & ctx, octogui::NodeHandle pane, std::string_view title)
        {
            fillWithChild(ctx, pane);

            const octogui::NodeHandle panel = ctx.createPanel(pane);
            auto layout = ctx.layoutStyle(panel);
            layout.mode = octogui::LayoutMode::Vertical;
            layout.horizontalAlignment = octogui::Alignment::Stretch;
            layout.gap = 6.0f;
            layout.padding = octogui::Padding{8.0f};

            const octogui::NodeHandle heading = ctx.createLabel(panel, title);
            ctx.style(heading).all.text = ctx.theme().textMuted;
            return panel;
        }

        void seeThrough(octogui::Context &ctx, octogui::NodeHandle node)
        {
            ctx.style(node).all.background = octogui::Color::transparent();
        }

        void fixedBar(octogui::Context &ctx, octogui::NodeHandle bar, octogui::f32 height)
        {
            auto layout = ctx.layoutStyle(bar);
            layout.mode = octogui::LayoutMode::Horizontal;
            layout.verticalAlignment = octogui::Alignment::Center;
            layout.gap = 6.0f;
            layout.padding = octogui::Padding{8.0f, 2.0f};
            layout.minSize = octogui::Vec2{0.0f, height};
            layout.preferredSize = octogui::Vec2{0.0f, height};
        }
    } // namespace

    void EditorShell::build(octogui::Context &ctx)
    {
        const octogui::NodeHandle root = ctx.ensureRoot();
        ctx.style(root).all.background = octogui::Color::transparent();
        auto rootLayout = ctx.layoutStyle(root);
        rootLayout.mode = octogui::LayoutMode::Vertical;
        rootLayout.horizontalAlignment = octogui::Alignment::Stretch;

        const octogui::NodeHandle topBar = ctx.createPanel(root);
        fixedBar(ctx, topBar, 36.0f);
        saveButton = ctx.createButton(topBar, "Save");
        playButton = ctx.createButton(topBar, "Play");

        // hierachy << scene << inspector
         const octogui::NodeHandle workspace =
        ctx.createSplitContainer(root, octogui::SplitOrientation::Horizontal, 0.18f);
        ctx.setSplitMinimumSizes(workspace, 180.0f, 600.0f);
        seeThrough(ctx, workspace);
        seeThrough(ctx, ctx.splitSecondPane(workspace));
        hierarchy = titledPanel(ctx, ctx.splitFirstPane(workspace), "Hierarchy");

        const octogui::NodeHandle sceneInspector =
        ctx.createSplitContainer(ctx.splitSecondPane(workspace), octogui::SplitOrientation::Horizontal, 0.72f);
        ctx.setSplitMinimumSizes(sceneInspector, 320.0f, 240.0f);
        seeThrough(ctx, sceneInspector);

        sceneNode = ctx.splitFirstPane(sceneInspector);
        seeThrough(ctx, sceneNode);

        sceneNode = ctx.splitFirstPane(sceneInspector);
        ctx.style(sceneNode).all.background = octogui::Color::transparent(); // a hole for the 3D scene until 3.3

        inspector = titledPanel(ctx, ctx.splitSecondPane(sceneInspector), "Inspector");

        const octogui::NodeHandle statusBar = ctx.createPanel(root);
        fixedBar(ctx, statusBar, 24.0f);
        statusLabel = ctx.createLabel(statusBar, "");
    }

    void EditorShell::update(octogui::Context &ctx, Engine &engine, Scribe::Session &session)
    {
        if (ctx.isClicked(saveButton))
        {
            const auto result = session.save();
            if (!result.ok())
            {
                Log::Print("SAVE FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
            }
        }

        if (ctx.isClicked(playButton))
        {
            const bool ok = engine.isPlaying() ? engine.stop() : engine.play();
            (void)ok;
        }

        const bool playing = engine.isPlaying();
        if (playing != lastPlaying)
        {
            ctx.setText(playButton, playing ? "Stop" : "Play");
            ctx.setEnabled(saveButton, !playing);
            lastPlaying = playing;
        }

        std::string status = "No scene";
        if (const auto *activeScene = engine.getActiveScene())
        {
            status = activeScene->getName() + (activeScene->isDirty() ? " *" : "") +
                    (playing ? "   |   Playing" : "   |   Editing");
        }

        if (status != lastStatus)
        {
            ctx.setText(statusLabel, status);
            lastStatus = std::move(status);
        }
    }
}
