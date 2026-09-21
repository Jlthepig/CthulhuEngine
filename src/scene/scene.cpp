#include "scene.h"
#include "light.h"
#include "log_utils.hpp"
#include "modelLoader.h"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene {
flecs::entity Scene::createEntity(const std::string &name) {
  auto e = world.entity(name.c_str());
  e.set(TransformComponent{});
  Log::Print("Created entity: " + name, "Scene", LogType::LOG_INFO);
  return e;
}

Rendering::Model *
Scene::getOrLoadModel(const std::string &resourcePath,
                      const std::filesystem::path &fileSystemPath) {
  auto it = modelCache.find(resourcePath);
  if (it != modelCache.end()) {
    Log::Print("Resuing model from cache: " + resourcePath, "Scene",
               LogType::LOG_INFO);
    return &it->second;
  } else {
    Log::Print("Loading model from file: " + resourcePath, "Scene",
               LogType::LOG_INFO);
    Rendering::Model model =
        Rendering::ModelLoader::loadGltf(fileSystemPath.string());
    modelCache[resourcePath] = std::move(model);
    return &modelCache[resourcePath];
  }
}

void Scene::clear() {
  world.delete_with<TransformComponent>();

  for (auto &[path, model] : modelCache) {
    model.destroy();
  }
  modelCache.clear();
  nextId = 0;

  directionalLight = Rendering::DirectionalLight{};
  pointLights.clear();

  Log::Print("Scene cleared", "Scene", LogType::LOG_INFO);
}
} // namespace Cthulhu::Scene
