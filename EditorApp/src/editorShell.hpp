#pragma once

#include <optional>
#include <string>
#include <vector>

#include "OctoGui/Event.h"
#include "OctoGui/Context.h"
#include "editorPanels.hpp"
#include "entityId.hpp"
#include "hierarchyPanel.hpp"
#include "inspectorPanel.hpp"
#include "viewportPanel.hpp"
#include "contentBrowserPanel.hpp"

using namespace octogui;

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scribe
{
class Session;
}

namespace Cthulhu::Scene
{
class Scene;
}

namespace Cthulhu::Editor
{
class EditorUi;
class EditorShell
{
  public:
    void build(Context &ctx);

    void update(Context &ctx, Engine &engine, Scribe::Session &session);

    void frame(EditorUi &ui, Engine &engine, Scribe::Session &session, void (*routeInput)(void *, float));

    [[nodiscard]] NodeHandle hierarchyPanel() const noexcept
    {
        return hierarchyPanelUI.panel();
    }

    [[nodiscard]] const ViewportPanel &viewport() const noexcept
    {
        return viewportPanelUI;
    }

    [[nodiscard]] NodeHandle sceneArea() const noexcept
    {
        return sceneNode;
    }

    [[nodiscard]] NodeHandle inspectorPanel() const noexcept
    {
        return inspector;
    }

    [[nodiscard]] NodeHandle sceneTree() const noexcept
    {
        return hierarchyPanelUI.treeView();
    }

    [[nodiscard]] std::optional<Scene::EntityId> selectedEntity() const noexcept
    {
        return hierarchyPanelUI.selectedEntity();
    }

  private:
    NodeHandle sceneLabel{};
    IconButton saveButton{};
    IconButton playButton{};
    IconButton pauseButton{};
    IconButton stopButton{};

    HierarchyPanel hierarchyPanelUI;
    ViewportPanel viewportPanelUI;
    NodeHandle sceneNode{};

    NodeHandle inspector{};
    InspectorPanel inspectorUI;
    ContentBrowserPanel contentBrowserUI;

    std::vector<octogui::UIEvent> uiEvents;

    std::string lastSceneText;
    bool lastPlaying = false;
    bool lastPaused = false;
};
} // namespace Cthulhu::Editor
