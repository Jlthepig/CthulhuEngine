#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "OctoGui/Context.h"
#include "entityId.hpp"
#include "inspectorPanel.hpp"

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
struct IconButton
{
    NodeHandle button{};
    NodeHandle icon{};
};

class EditorShell
{
  public:
    void build(Context &ctx);

    void update(Context &ctx, Engine &engine, Scribe::Session &session);

    [[nodiscard]] NodeHandle hierarchyPanel() const noexcept
    {
        return hierarchy;
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
        return hierarchyTree;
    }

  private:
    NodeHandle sceneLabel{};
    NodeHandle saveButton{};
    IconButton playButton{};
    IconButton pauseButton{};
    IconButton stopButton{};


    NodeHandle hierarchy{};
    NodeHandle hierarchyHeader{};
    NodeHandle hierarchyTree{};
    NodeHandle sceneNode{};
    IconButton createEntityButton{};

    NodeHandle inspector{};
    InspectorPanel inspectorUI;

    std::string lastSceneText;
    bool lastPlaying = false;
    bool lastPaused = false;

    std::vector<std::pair<std::uint64_t, Scene::EntityId>> hierarchyEntityTokens;
    std::optional<Scene::EntityId> selectedEntity;
    std::uint64_t nextHierarchyToken = 1;
    std::uint64_t hierarchySceneGeneration = 0;
    bool hierarchyInitialized = false;

    void rebuildHierarchy(Context &ctx, Scene::Scene &scene, std::optional<Scene::EntityId> restoreSelection);
    void updateSelection(Context &ctx);
};
} // namespace Cthulhu::Editor