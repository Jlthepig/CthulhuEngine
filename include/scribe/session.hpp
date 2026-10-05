#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "command.hpp"
#include "componentRegistry.hpp"
#include "history.hpp"

namespace Cthulhu::Scribe
{
    // << shalll not be used by runtime code! >>
class Session
{
  public:
    explicit Session(Engine &engine);

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    Result execute(std::unique_ptr<Command> command);

    bool undo();
    bool redo();

    [[nodiscard]] bool canUndo() const noexcept
    {
        return history.canUndo();
    }

    [[nodiscard]] bool canRedo() const noexcept
    {
        return history.canRedo();
    }

    [[nodiscard]] std::string_view undoName() const noexcept
    {
        return history.undoName();
    }

    [[nodiscard]] std::string_view redoName() const noexcept
    {
        return history.redoName();
    }

    // must be called when a drag or typing interaction of some kind finishes thus the next edit becomes its own undo step
    void endMerge() noexcept
    {
        history.endMerge();
    }

    Result save();
    Result saveAs(std::string_view resourcePath);

    [[nodiscard]] std::vector<ChangeEvent> takeEvents();

    Result renameScene(std::string_view newName);

    EntityResult createEntity(std::string_view name, std::optional<Scene::EntityId> parent = std::nullopt);
    Result deleteEntity(Scene::EntityId id);
    EntityResult duplicateEntity(Scene::EntityId id);
    Result renameEntity(Scene::EntityId id, std::string_view newName);
    Result reparentEntity(Scene::EntityId child, std::optional<Scene::EntityId> newParent);
    Result addComponent(Scene::EntityId id, std::string_view component);
    Result removeComponent(Scene::EntityId id, std::string_view component);

    Result setField(Scene::EntityId id, std::string_view component, std::string_view field, const Scene::FieldValue& value);
    
    // << assets applied immediately never undoable >>
    AssetResult importAsset(const std::filesystem::path& sourceFile, std::string_view destination);
    
    Result moveAsset(Assets::AssetId id, std::string_view destination);

    Result replaceReferences(std::string_view from, std::string_view to);

    Result deleteAsset(Assets::AssetId id);

  private:
    Engine &engine;
    History history;
    std::vector<ChangeEvent> events;
    uint64_t boundGeneration = 0;

    Scene::Scene *syncScene();
    void refreshDirty(Scene::Scene &scene);
    void rewriteReferences(Scene::Scene &scene, std::string_view from, std::string_view to);
};
} // namespace Cthulhu::Scribe