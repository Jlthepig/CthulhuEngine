#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "OctoGui/Context.h"
#include "entityId.hpp"

namespace Cthulhu { class Engine; }
namespace Cthulhu::Scribe { class Session; }
namespace Cthulhu::Scene { class Scene; }

namespace Cthulhu::Editor {
class HierarchyPanel {
public:
  void build(octogui::Context &ctx, octogui::NodeHandle parent);
  void update(octogui::Context &ctx, Engine &engine, Scribe::Session &session);

  [[nodiscard]] octogui::NodeHandle panel() const noexcept { return hierarchy; }
  [[nodiscard]] octogui::NodeHandle treeView() const noexcept { return hierarchyTree; }
  [[nodiscard]] std::optional<Scene::EntityId> selectedEntity() const noexcept { return selectedEntity_; }

  void selectEntity(octogui::Context &ctx, std::optional<Scene::EntityId> entity);

private:
  octogui::NodeHandle hierarchy{};
  octogui::NodeHandle hierarchyHeader{};
  octogui::NodeHandle hierarchyTree{};
  octogui::NodeHandle createEntityButton{};
  octogui::NodeHandle createEntityIcon{};

  std::vector<std::pair<std::uint64_t, Scene::EntityId>> hierarchyEntityTokens;
  std::vector<std::pair<Scene::EntityId, octogui::TreeViewItemId>> hierarchyEntityItems;
  std::optional<Scene::EntityId> selectedEntity_;
  std::uint64_t nextHierarchyToken = 1;
  std::uint64_t hierarchySceneGeneration = 0;
  bool hierarchyInitialized = false;
  bool resetHierarchyExpansion = false;

  octogui::NodeHandle hierarchyRenameInput{};
  std::optional<Scene::EntityId> hierarchyRenameEntity;
  octogui::TreeViewItemId hierarchyRenameItem{};
  octogui::LayoutMode hierarchyTreeLayoutModeBeforeRename = octogui::LayoutMode::Vertical;
  bool hierarchyTreeClipBeforeRename = false;

  std::optional<Scene::EntityId> lastHierarchyClickEntity;
  float hierarchyClickAge = 1.0f;

  void rebuildHierarchy(octogui::Context &ctx, Scene::Scene &scene, std::optional<Scene::EntityId> restoreSelection);
  void updateSelection(octogui::Context &ctx);
};
}