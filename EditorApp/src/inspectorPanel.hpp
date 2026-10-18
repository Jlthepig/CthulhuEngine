#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "OctoGui/Context.h"
#include "componentRegistry.hpp"

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
class InspectorPanel
{
  public:
    void build(octogui::Context &ctx, octogui::NodeHandle panel);
    void update(octogui::Context &ctx, Engine &engine, Scribe::Session &session,
                Scene::Scene *scene, std::optional<Scene::EntityId> selectedEntity);

  private:
    struct Binding
    {
        octogui::NodeHandle control{};
        std::string component;
        std::string field;
        Scene::FieldType type = Scene::FieldType::Float;
        Scene::FieldHint hint = Scene::FieldHint::None;
        std::size_t axis = 0;
        double shownValue = 0.0;
        std::vector<Scene::FieldValue> options;
    };

    octogui::NodeHandle panel{};
    octogui::NodeHandle content{};
    octogui::NodeHandle generatedContent{};
    std::vector<Binding> bindings;
    std::vector<std::string> componentNames;
    std::optional<Scene::EntityId> selectedEntity;
    std::optional<Scene::EntityId> builtEntity;
    bool initialized = false;

    void resetContent(octogui::Context &ctx);
    void rebuild(octogui::Context &ctx, Engine &engine, Scene::Scene *scene);
    void processEvents(octogui::Context &ctx, Engine &engine, Scribe::Session &session);
    void syncTransform(octogui::Context &ctx, Engine &engine, Scribe::Session &session);
    void refreshValues(octogui::Context &ctx, Engine &engine);
};
} // namespace Cthulhu::Editor
