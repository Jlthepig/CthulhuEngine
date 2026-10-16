#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "OctoGui/Context.h"
#include "entityId.hpp"

using namespace octogui;

namespace Cthulhu::Scene
{
class Scene;
}
namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scribe
{
class Session;
}

namespace Cthulhu::Editor
{
// the editor's top bar, resizable workspace panes, status bar

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
    IconButton stepButton{};


    NodeHandle hierarchy{};
    NodeHandle hierarchyHeader{};
    NodeHandle hierarchyTree{};
    NodeHandle sceneNode{};
    NodeHandle inspector{};

    std::string lastSceneText;
    bool lastPlaying = false;
    bool lastPaused = false;


    std::vector<std::pair<std::uint64_t, Scene::EntityId>> hierarchyEntityTokens;
    std::optional<Scene::EntityId> selectedEntity;
    std::uint64_t nextHierarchyToken = 1;
    std::uint64_t hierarchySceneGeneration{};
    bool hierarchyInitialized = false;

    void rebuildHierarchy(Context &ctx, Scene::Scene &scene,std::optional<Scene::EntityId> restoreSelection);
    void updateSelection(Context &ctx);
};
} // namespace Cthulhu::Editor