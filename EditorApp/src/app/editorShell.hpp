#pragma once

#include <vector>
#include <optional>
#include <filesystem>
#include <cstdint>
#include <string>

#include "OctoGui/Event.h"
#include "OctoGui/Context.h"
#include "editorPanels.hpp"
#include "hierarchyPanel.hpp"
#include "inspectorPanel.hpp"
#include "viewportPanel.hpp"
#include "contentBrowserPanel.hpp"
#include "assetDrag.hpp"
#include "editorDialog.hpp"
#include "camera.hpp"
#include "platform.hpp"
#include "session.hpp"

using namespace octogui;

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scribe
{
class Session;
struct SceneDocument;
struct Result;
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

    [[nodiscard]] bool frame(EditorUi &ui, Engine &engine, Scribe::Session &session, void (*routeInput)(void *, float));

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
    struct SceneTab
    {
        NodeHandle root{}, button{}, close{};
        Scribe::SceneDocument *document = nullptr;
        std::vector<Scene::EntityId> selection;
        std::optional<Scene::Camera> camera;
    };

    void buildSceneTabBar(Context &ctx, NodeHandle parent);
    void setPlayInputCaptured(Engine &engine, bool captured);
    void updatePlayControls(Context &ctx, Engine &engine);
    void addSceneTab(Context &ctx);
    void rememberSceneView(Engine &engine);
    void restoreSceneView(Engine &engine);
    void updateSceneTabs(Context &ctx, Engine &engine, Scribe::Session &session);
    Scribe::Result selectSceneTab(Engine &engine, Scribe::Session &session, std::size_t index, bool keepCurrent = true);
    Scribe::Result closeSceneTab(Context &ctx, Engine &engine, Scribe::Session &session,
                                 std::size_t index, std::size_t returnTab);
    void handleSceneTabClose(Context &ctx, Engine &engine, Scribe::Session &session, DialogChoice choice);

    Core::Platform::FileDialogResult pickSceneFile(Context &ctx, Engine &engine, Scribe::Session &session, bool saving);

    std::optional<std::size_t> findSceneTab(Engine &engine, std::string_view path, std::optional<std::size_t> skip = std::nullopt) const;

    Scribe::Result openSceneTab(Context &ctx, Engine &engine, Scribe::Session &session, std::string_view path);

    void openSceneFile(Context &ctx, Engine &engine, Scribe::Session &session);

    Scribe::Result saveScene(Context &ctx, Engine &engine, Scribe::Session &session, bool saveAs = false);

    Scribe::Result saveSceneTabs(Context &ctx, Engine &engine, Scribe::Session &session);

    std::vector<SceneTab> sceneTabs;
    NodeHandle tabBar{}, tabRow{}, previousTabs{}, nextTabs{};
    std::size_t activeTab{};
    std::size_t firstVisibleTab{};
    std::optional<std::size_t> closingTab;
    std::size_t closeReturnTab{};
    bool restoreTabSelection = false;

    bool updateFileMenu(Context &ctx, Engine &engine, Scribe::Session &session);
    Scribe::Result newSceneTab(Context &ctx, Engine &engine, Scribe::Session &session);
    void requestSceneTabClose(Context &ctx, Engine &engine, Scribe::Session &session, std::size_t index);
    Scribe::Result closeAllSceneTabs(Context &ctx, Engine &engine, Scribe::Session &session);
    void rememberSceneFile(Engine &engine, std::string_view path);
    

    NodeHandle fileButton{};
    std::vector<std::string> recentScenes;
    std::filesystem::path recentProject;
    std::uint64_t fileMenuGeneration{};
    bool closingAllScenes = false;

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

    bool lastPlaying = false;
    bool lastPaused = false;
    bool playInputCaptured = false;
    std::uint64_t capturedSceneGeneration{};

    bool handleExit(Context &ctx, Engine &engine, Scribe::Session &session);
    bool beginAssetDelete(Context &ctx, Engine &engine, Scribe::Session &session);
    void showAssetDelete(Context &ctx, Engine &engine, Scribe::Session &session);
    void handleAssetDelete(Context &ctx, Engine &engine, Scribe::Session &session);

    bool beginReferenceRepair(Context &ctx, Engine &engine, Scribe::Session &session, bool manual, std::string_view source = {});
    void showReferenceRepair(Context &ctx, Engine &engine);
    void showRepairGroup(Context &ctx, Engine &engine);
    void handleReferenceRepair(Context &ctx, Engine &engine, Scribe::Session &session);
    void finishReferenceRepair(Context &ctx, Engine &engine, Scribe::Session &session);

    NodeHandle missingAssetsButton{};
    NodeHandle cameraStatusLabel{};
    Scene::GameCameraStatus displayedCameraStatus = Scene::GameCameraStatus::Editor;
    bool missingCheckPending = true;
    std::uint64_t checkedMissingGeneration{};

    bool referenceRepairOpen = false;
    std::uint64_t repairGeneration{};
    std::filesystem::path repairProject;
    std::string repairSource;
    std::vector<Scribe::MissingAsset> repairGroups;
    std::vector<std::string> repairTargets;
    int repairGroup = -1;

    Assets::AssetId deletingAsset{};
    std::string deletingPath;
    std::filesystem::path deletingProject;
    std::uint64_t deletingGeneration{};
    bool deletingMissing = false;

    EditorDialog exitDialog;
    bool resumePlayOnCancel = false;
};
} // namespace Cthulhu::Editor
