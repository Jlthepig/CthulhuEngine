#pragma once

#include <optional>
#include <string>

#include "OctoGui/Context.h"
#include "entityId.hpp"
#include "hierarchyPanel.hpp"
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
        return hierarchyPanel_.panel();
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
        return hierarchyPanel_.treeView();
    }
    [[nodiscard]] std::optional<Scene::EntityId> selectedEntity() const noexcept
    {
        return hierarchyPanel_.selectedEntity();
    }

  private:
    NodeHandle sceneLabel{};
    NodeHandle saveButton{};
    IconButton playButton{};
    IconButton pauseButton{};
    IconButton stopButton{};


    HierarchyPanel hierarchyPanel_;
    NodeHandle sceneNode{};

    NodeHandle inspector{};
    InspectorPanel inspectorUI;

    std::string lastSceneText;
    bool lastPlaying = false;
    bool lastPaused = false;

};
} // namespace Cthulhu::Editor