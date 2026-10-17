#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "OctoGui/Context.h"
#include "componentRegistry.hpp"

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
    IconButton stepButton{};


    NodeHandle hierarchy{};
    NodeHandle hierarchyHeader{};
    NodeHandle hierarchyTree{};
    NodeHandle sceneNode{};
    NodeHandle inspector{};
    NodeHandle inspectorContent{};

    std::string lastSceneText;
    bool lastPlaying = false;
    bool lastPaused = false;

    std::vector<std::pair<std::uint64_t, Scene::EntityId>> hierarchyEntityTokens;
    std::optional<Scene::EntityId> selectedEntity;
    std::uint64_t nextHierarchyToken = 1;
    std::uint64_t hierarchySceneGeneration = 0;
    bool hierarchyInitialized = false;

    struct InspectorBinding
    {
        NodeHandle control{};
        std::string component;
        std::string field;
        Scene::FieldType type = Scene::FieldType::Float;
        Scene::FieldHint hint = Scene::FieldHint::None;
        std::size_t axis = 0;
        double shownValue = 0.0;
        std::vector<Scene::FieldValue> options;
    };

    std::vector<NodeHandle> inspectorChildren;
    std::vector<InspectorBinding> inspectorBindings;
    std::vector<std::string> inspectorComponentNames;
    std::optional<Scene::EntityId> inspectorEntity;
    bool inspectorInitialized = false;

    void rebuildHierarchy(Context &ctx, Scene::Scene &scene, std::optional<Scene::EntityId> restoreSelection);
    void updateSelection(Context &ctx);
    void rebuildInspector(Context &ctx, Engine &engine, Scene::Scene *scene);
    void processInspectorEvents(Context &ctx, Engine &engine, Scribe::Session &session);
    void syncTransformInputs(Context &ctx, Engine &engine, Scribe::Session &session);
    void refreshInspectorValues(Context &ctx, Engine &engine);
};
} // namespace Cthulhu::Editor