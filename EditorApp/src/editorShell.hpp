#pragma once

#include <string>
#include <vector>
#include <optional>

#include "OctoGui/Event.h"
#include "OctoGui/Context.h"
#include "editorPanels.hpp"
#include "hierarchyPanel.hpp"
#include "inspectorPanel.hpp"
#include "viewportPanel.hpp"
#include "contentBrowserPanel.hpp"
#include "assetDrag.hpp"

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

    void clearAssetDrag(Engine &engine);

    [[nodiscard]] const ViewportPanel &viewport() const noexcept
    {
        return viewportPanelUI;
    }

    [[nodiscard]] NodeHandle sceneArea() const noexcept
    {
        return sceneNode;
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

    InspectorPanel inspectorUI;
    ContentBrowserPanel contentBrowserUI;

    AssetDragState assetDrag;
    DrawList assetFeedback;

    std::optional<glm::vec3> assetPreviewPosition;
    Scene::EntityId pendingAssetSelection{};
    std::uint64_t pendingAssetSelectionGeneration{};

    void updateAssetDrag(Context &ctx, Engine &engine, Scribe::Session &session);
    AssetDropFeedback dropInViewport(Context &ctx, Engine &engine, Scribe::Session &session, const Assets::AssetRecord &asset, bool released);

    std::vector<octogui::UIEvent> uiEvents;

    std::string lastSceneText;
    bool lastPlaying = false;
    bool lastPaused = false;
};
} // namespace Cthulhu::Editor
